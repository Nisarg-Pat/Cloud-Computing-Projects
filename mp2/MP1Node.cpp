/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
        addAddressToMemberList(joinaddr, 0);
    }
    else {
        sendMessage(joinaddr, JOINREQ);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**

*/
void MP1Node::sendMessage(Address* receiveAddr, MsgTypes msgType) {
	MessageHdr *msg;

	#ifdef DEBUGLOG
        static char s[1024];
    #endif

    if (msgType == JOINREQ) {
        size_t msgsize = sizeof(MessageHdr) + sizeof(Address) + sizeof(long);
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr, sizeof(Address));
        memcpy((char *)(msg+1) + sizeof(Address), &memberNode->heartbeat, sizeof(long));
        //cout<<memberNode->addr.getAddress()<<" "<<memberNode->heartbeat<<"\n";

        #ifdef DEBUGLOG
            sprintf(s, "Trying to join...");
            log->LOG(&memberNode->addr, s);
        #endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, receiveAddr, (char *)msg, msgsize);

    } else if (msgType == JOINREP) {
        int memberListSize = memberNode->memberList.size();
        size_t msgsize = sizeof(MessageHdr) + sizeof(Address) + sizeof(int) + (memberListSize * sizeof(MemberListEntry));
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        msg->msgType = JOINREP;
        memcpy((char *)(msg+1), &memberNode->addr, sizeof(Address));
        memcpy((char *)(msg+1) + sizeof(Address), &memberListSize, sizeof(int));
        for(int i=0;i<memberListSize;i++) {
            memcpy((char *)(msg+1) + sizeof(Address) + sizeof(int) + (i*sizeof(MemberListEntry)), &memberNode->memberList[i], sizeof(MemberListEntry));
        }

        #ifdef DEBUGLOG
            string ss = receiveAddr->getAddress();
            log->LOG(&memberNode->addr, "Sending JOINREP to: %s. memberlist size = %d", ss.c_str(), memberListSize);
        #endif

        emulNet->ENsend(&memberNode->addr, receiveAddr, (char *)msg, msgsize);
    } else if (msgType == PING) {
        int memberListSize = memberNode->memberList.size();
        size_t msgsize = sizeof(MessageHdr) + sizeof(Address) + sizeof(int) + (memberListSize * sizeof(MemberListEntry));
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        msg->msgType = PING;
        memcpy((char *)(msg+1), &memberNode->addr, sizeof(Address));
        memcpy((char *)(msg+1) + sizeof(Address), &memberListSize, sizeof(int));
        for(int i=0;i<memberListSize;i++) {
            memcpy((char *)(msg+1) + sizeof(Address) + sizeof(int) + (i*sizeof(MemberListEntry)), &memberNode->memberList[i], sizeof(MemberListEntry));
        }

        emulNet->ENsend(&memberNode->addr, receiveAddr, (char *)msg, msgsize);
    }
    free(msg);
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
	 MessageHdr* msg = (MessageHdr *) data;
	 if(msg->msgType == JOINREQ) {

	    //GET THE MESSAGE
	    Address* sendAddress = new Address();
	    long heartbeat;

	    memcpy(sendAddress, (char *)(msg+1), sizeof(sendAddress));
	    memcpy(&heartbeat, (char *)(msg+1) + sizeof(sendAddress), sizeof(long));
	    //cout<<addr.getAddress()<<" "<<value<<"\n";

        #ifdef DEBUGLOG
            string ss = sendAddress->getAddress();
            log->LOG(&memberNode->addr, "Received JOINREQ from %s having heartbeats = %d", ss.c_str(), heartbeat);
        #endif
        log->logNodeAdd(&memberNode->addr, sendAddress);

        //PERFORM JOINREQ OPERATIONS
        addAddressToMemberList(sendAddress, heartbeat);

        //SEND RESPONSE
        sendMessage(sendAddress, JOINREP);

        //FREE ANY MEMORY USED
        free(sendAddress);
	 } else if (msg -> msgType == JOINREP) {
	    Address* sendAddress = new Address();
	    int memberListSize;
	    MemberListEntry entry;

	    memcpy(sendAddress, (char *)(msg+1), sizeof(Address));
        memcpy(&memberListSize, (char *)(msg+1) + sizeof(Address), sizeof(int));

        #ifdef DEBUGLOG
            string ss = sendAddress->getAddress();
            log->LOG(&memberNode->addr, "Received JOINREP from %s having memberlist size = %d", ss.c_str(), memberListSize);
        #endif

        if(memberListSize > 0) {
            memberNode->inGroup = true;
        }

        for(int i=0;i<memberListSize;i++) {
            memcpy(&entry, (char *)(msg+1) + sizeof(Address) + sizeof(int) + (i*sizeof(MemberListEntry)), sizeof(MemberListEntry));
            addEntryToMemberList(entry);
        }

        free(sendAddress);
	 } else if (msg -> msgType == PING) {
        Address* sendAddress = new Address();
        int memberListSize;
        MemberListEntry entry;

        memcpy(sendAddress, (char *)(msg+1), sizeof(Address));
        memcpy(&memberListSize, (char *)(msg+1) + sizeof(Address), sizeof(int));

        for(int i=0;i<memberListSize;i++) {
            memcpy(&entry, (char *)(msg+1) + sizeof(Address) + sizeof(int) + (i*sizeof(MemberListEntry)), sizeof(MemberListEntry));
            updateMembership(entry);
        }
	 }
}

bool MP1Node::addAddressToMemberList(Address* address, long heartbeat) {
    int id = getIdFromAddress(address);
    short port = getPortFromAddress(address);
    long timestamp = par->getcurrtime();
    MemberListEntry member(id, port, heartbeat, timestamp);
    return addEntryToMemberList(member);
}

bool MP1Node::addEntryToMemberList(MemberListEntry entry) {
    if(checkMembership(entry.getid(),entry.getport())) {
        return false;
    }
    #ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Adding following to memberlist: %d:%d, %d, %d", entry.getid(), entry.getport(), entry.getheartbeat(), entry.gettimestamp());
    #endif
    Address* address = getAddressFromIdPort(entry.getid(),entry.getport());
    log->logNodeAdd(&memberNode->addr, address);
    memberNode->memberList.push_back(entry);
    return true;
}

void MP1Node::updateMembership(MemberListEntry entry) {
    MemberListEntry* current = getMembership(entry.getid(), entry.getport());
    if(current == nullptr) {
        addEntryToMemberList(entry);
    } else {
        if(entry.getheartbeat() > current->getheartbeat()) {
            current->heartbeat = entry.getheartbeat();
            current->timestamp = par->getcurrtime();
        }
    }
}

bool MP1Node::checkMembership(int id, short port) {
    for(int i=0; i< memberNode->memberList.size(); i++) {
        if( memberNode->memberList[i].id == id && memberNode->memberList[i].port == port) {
            return true;
        }
    }
    return false;
}

MemberListEntry* MP1Node::getMembership(int id, short port) {
    for(int i=0; i< memberNode->memberList.size(); i++) {
        if( memberNode->memberList[i].id == id && memberNode->memberList[i].port == port) {
            return &memberNode->memberList[i];
        }
    }
    return nullptr;
}

string MP1Node::printMembership() {
    string out = "Membership of "+memberNode->addr.getAddress()+" Size = "+to_string(memberNode->memberList.size())+"\n";
    for(int i=0; i< memberNode->memberList.size(); i++) {
        out = out+to_string(memberNode->memberList[i].id)+" "+to_string(memberNode->memberList[i].port)
        +" "+to_string(memberNode->memberList[i].heartbeat)+" "+to_string(memberNode->memberList[i].timestamp)+"\n";
    }
    return out;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

    /*
    * Your code goes here
    */
    memberNode->heartbeat++;

    MemberListEntry *current = getMembership(getIdFromAddress(&memberNode->addr), getPortFromAddress(&memberNode->addr));
    current->heartbeat++;
    current->timestamp = par->getcurrtime();

    for(int i=memberNode->memberList.size()-1;i>=0; i--) {
        if(par->getcurrtime() - memberNode->memberList[i].timestamp >= TREMOVE) {
            #ifdef DEBUGLOG
                log->LOG(&memberNode->addr, "Deleting %d:%d, H=%d, T=%d from memberlist.",
                memberNode->memberList[i].getid(), memberNode->memberList[i].getport(),
                memberNode->memberList[i].getheartbeat(), memberNode->memberList[i].gettimestamp());
            #endif
            Address* address = getAddressFromIdPort(memberNode->memberList[i].getid(),memberNode->memberList[i].getport());
            log->logNodeRemove(&memberNode->addr, address);
            memberNode->memberList.erase(memberNode->memberList.begin() + i);
        }
    }

//        #ifdef DEBUGLOG
//            log->LOG(&memberNode->addr, "%s", printMembership().c_str());
//        #endif

    for(int i=memberNode->memberList.size()-1;i>=0; i--) {
        Address *receiveAddr = getAddressFromIdPort(memberNode->memberList[i].getid(), memberNode->memberList[i].getport());
        sendMessage(receiveAddr, PING);
    }

    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;
}

int MP1Node::getIdFromAddress(Address *address) {
    int id = 0;
    memcpy(&id, &address->addr[0], sizeof(int));
    return id;
}

short MP1Node::getPortFromAddress(Address *address) {
    short port;
    memcpy(&port, &address->addr[4], sizeof(short));
    return port;
}

Address* MP1Node::getAddressFromIdPort(int id, short port) {
    Address* address = new Address();
    memcpy(&address->addr[0], &id, sizeof(int));
    memcpy(&address->addr[4], &port, sizeof(short));
    return address;
}
