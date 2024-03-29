/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/
#include "MP2Node.h"

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
//	map<int, Transaction*>::iterator it = transactionTable.begin();
//    while(it != transactionTable.end()) {
//        delete it->second;
//        it++;
//    }
}

/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	vector<Node> curMemList;
	bool change = false;

	/*
	 *  Step 1. Get the current membership list from Membership Protocol / MP1
	 */
	curMemList = getMembershipList();
//	#ifdef DEBUGLOG
//        for(int i=0;i<curMemList.size(); i++) {
//            string ss = curMemList[i].getAddress()->getAddress();
//            log->LOG(&memberNode->addr, "Ring Element: %s :::: %d", ss.c_str(), curMemList[i].getHashCode());
//        }
//    #endif

	/*
	 * Step 2: Construct the ring
	 */
	// Sort the list based on the hashCode
	sort(curMemList.begin(), curMemList.end());

    if(this->ring.size()!=curMemList.size()) {
        change = true;
    } else {
        for(int i=0;i<this->ring.size(); i++) {
            if(this->ring[i].getHashCode() != curMemList[i].getHashCode()) {
                change = true;
                break;
            }
        }
    }

//    #ifdef DEBUGLOG
//        for(int i=0;i<curMemList.size(); i++) {
//            string ss = curMemList[i].getAddress()->getAddress();
//            log->LOG(&memberNode->addr, "Ring Element: %s :::: %d", ss.c_str(), curMemList[i].getHashCode());
//        }
//    #endif

    if(change) {
        this->ring = curMemList;
        int myPos = -1;
        for(int i=0;i<this->ring.size(); i++) {
            if(this->ring[i].getHashCode() == hashFunction(memberNode->addr.addr)) {
                myPos = i;
            }
        }

        this->hasMyReplicas.clear();
        this->hasMyReplicas.emplace_back(this->ring[(myPos+1)%this->ring.size()]);
        this->hasMyReplicas.emplace_back(this->ring[(myPos+2)%this->ring.size()]);
        this->haveReplicasOf.clear();
        this->haveReplicasOf.emplace_back(this->ring[(myPos+this->ring.size()-2)%this->ring.size()]);
        this->haveReplicasOf.emplace_back(this->ring[(myPos+this->ring.size()-1)%this->ring.size()]);

        #ifdef DEBUGLOG
            log->LOG(&memberNode->addr, "Ring Changed!!! MyPos in Ring: %d", myPos);
            log->LOG(&memberNode->addr, "hasMyReplicas :::: %d", this->hasMyReplicas[0].getHashCode());
            log->LOG(&memberNode->addr, "hasMyReplicas :::: %d", this->hasMyReplicas[1].getHashCode());
            log->LOG(&memberNode->addr, "haveReplicasOf :::: %d", this->haveReplicasOf[0].getHashCode());
            log->LOG(&memberNode->addr, "haveReplicasOf :::: %d", this->haveReplicasOf[1].getHashCode());
            for(int i=0;i<this->ring.size(); i++) {
                string ss = this->ring[i].getAddress()->getAddress();
                log->LOG(&memberNode->addr, "Ring Element: %s :::: %d", ss.c_str(), this->ring[i].getHashCode());
            }
        #endif
    }


	/*
	 * Step 3: Run the stabilization protocol IF REQUIRED
	 */
	// Run stabilization protocol if the hash table size is greater than zero and if there has been a changed in the ring
	if(!ht->isEmpty() && change) {
       stabilizationProtocol();
	}
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {
	/*
	 * Implement this
	 */
	 vector<Node> replicas = findNodes(key);
	 int transId = transactionTable.size();
	 createTransaction(transId, CREATE, key, value);
	 Message message(transId, memberNode->addr, CREATE, key, value);
	 for(int i=0;i<replicas.size();i++) {
	    emulNet->ENsend(&memberNode->addr, replicas[i].getAddress(), message.toString());
	 }
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
	/*
	 * Implement this
	 */
	 vector<Node> replicas = findNodes(key);
	 int transId = transactionTable.size();
     createTransaction(transId, READ, key, "");
     Message message(transId, memberNode->addr, READ, key);
     for(int i=0;i<replicas.size();i++) {
        emulNet->ENsend(&memberNode->addr, replicas[i].getAddress(), message.toString());
     }
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
	/*
	 * Implement this
	 */
	 vector<Node> replicas = findNodes(key);
	 int transId = transactionTable.size();
     createTransaction(transId, UPDATE, key, value);
     Message message(transId, memberNode->addr, UPDATE, key, value);
     for(int i=0;i<replicas.size();i++) {
        emulNet->ENsend(&memberNode->addr, replicas[i].getAddress(), message.toString());
     }
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
	/*
	 * Implement this
	 */
    vector<Node> replicas = findNodes(key);
    int transId = transactionTable.size();
    createTransaction(transId, DELETE, key, "");
    Message message(transId, memberNode->addr, DELETE, key);
    for(int i=0;i<replicas.size();i++) {
        emulNet->ENsend(&memberNode->addr, replicas[i].getAddress(), message.toString());
    }
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica, int transId) {
	/*
	 * Implement this
	 */
	// Insert key, value, replicaType into the hash table
	Entry entry(value, 0, replica);
	bool added = ht->create(key, entry.convertToString());
	if(transId!=REPLICATE) {
	    outputLog(CREATE, false, transId, key, value, added);
	}
    return added;
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key, int transId) {
	/*
	 * Implement this
	 */
	// Read key from local hash table and return value
	string val = ht->read(key);
    if(val != "") {
        Entry entry(val);
        outputLog(READ, false, transId, key, entry.value, true);
        return entry.value;
    } else {
        outputLog(READ, false, transId, key, "", false);
        return val;
    }
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica, int transId) {
	/*
	 * Implement this
	 */
	// Update key in local hash table and return true or false
    Entry entry(value, 0, replica);
    bool updated = ht->update(key, entry.convertToString());
    outputLog(UPDATE, false, transId, key, value, updated);
    return updated;
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key, int transId) {
	/*
	 * Implement this
	 */
	// Delete the key from the local hash table
    bool deleted = ht->deleteKey(key);
    outputLog(DELETE, false, transId, key, "", deleted);
    return deleted;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	char * data;
	int size;

	/*
	 * Declare your local variables here
	 */

	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		string messageString(data, data + size);
		Message message(messageString);

//        #ifdef DEBUGLOG
//            log->LOG(&memberNode->addr, "Received message %s", messageString.c_str());
//        #endif

        /*
         * Handle the message types here
         */

		if(message.type == CREATE) {
            bool val = createKeyValue(message.key, message.value, message.replica, message.transID);
            if(message.transID!=REPLICATE) {
                Message replyMessage(message.transID, memberNode->addr, REPLY, val);
                emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
            }
		} else if(message.type == READ) {
		    string val = readKey(message.key, message.transID);
		    Message replyMessage(message.transID, memberNode->addr, val);
            emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
		} else if(message.type == UPDATE) {
		    bool val = updateKeyValue(message.key, message.value, message.replica, message.transID);
		    Message replyMessage(message.transID, memberNode->addr, REPLY, val);
            emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
		} else if(message.type == DELETE) {
		    bool val = deletekey(message.key, message.transID);
		    Message replyMessage(message.transID, memberNode->addr, REPLY, val);
            emulNet->ENsend(&memberNode->addr, &message.fromAddr, replyMessage.toString());
		} else if(message.type == REPLY) {
		    transactionTable[message.transID]->replyCount++;
		    if(message.success) {
		        transactionTable[message.transID]->successCount++;
		        if(transactionTable[message.transID]->successCount == 2) {
		            outputLog(transactionTable[message.transID]->type, true, message.transID,
		            transactionTable[message.transID]->key, transactionTable[message.transID]->value, true);
		        }
		    }
		    if(transactionTable[message.transID]->replyCount == 3 && transactionTable[message.transID]->successCount <=1) {
                outputLog(transactionTable[message.transID]->type, true, message.transID,
                transactionTable[message.transID]->key, transactionTable[message.transID]->value, false);
            }
		} else if(message.type == READREPLY) {
		    transactionTable[message.transID]->replyCount++;
            if(message.value != "") {
                transactionTable[message.transID]->successCount++;
                transactionTable[message.transID]->value = message.value;
                if(transactionTable[message.transID]->successCount == 2) {
                    outputLog(transactionTable[message.transID]->type, true, message.transID,
                    transactionTable[message.transID]->key, transactionTable[message.transID]->value, true);
                }
            }
            if(transactionTable[message.transID]->replyCount == 3 && transactionTable[message.transID]->successCount <=1) {
                outputLog(transactionTable[message.transID]->type, true, message.transID,
                transactionTable[message.transID]->key, transactionTable[message.transID]->value, false);
            }
		}
		for(int i=0;i<transactionTable.size();i++) {
		    if(par->getcurrtime() - transactionTable[i]->timestamp >= TIMEOUT && transactionTable[i]->replyCount < 3) {
                transactionTable[i]->replyCount = 3;
                if(transactionTable[i]->successCount <=1) {
                   outputLog(transactionTable[i]->type, true, transactionTable[i]->id,
                   transactionTable[i]->key, transactionTable[i]->value, false);
                }
		    }
		}

	}

	/*
	 * This function should also ensure all READ and UPDATE operation
	 * get QUORUM replies
	 */
}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}
/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() {
    map<string, string>::iterator it;
    for(it = ht->hashTable.begin(); it != ht->hashTable.end(); it++) {
        string key = it->first;
        string value = it->second;
        vector<Node> replicas = findNodes(key);
        Message message(REPLICATE, memberNode->addr, CREATE, key, value);
         for(int i=0;i<replicas.size();i++) {
            emulNet->ENsend(&memberNode->addr, replicas[i].getAddress(), message.toString());
         }
    }
}

void MP2Node::createTransaction(int transId, MessageType msgType, string key, string value) {
    Transaction *transaction = new Transaction(transId, par->getcurrtime(), msgType, key, value);
    transactionTable.emplace_back(transaction);
}

void MP2Node::outputLog(MessageType type, bool isCoordinator, int transID, string key, string value, bool success) {
    if(type == CREATE) {
        if(success) {
            log->logCreateSuccess(&memberNode->addr, isCoordinator, transID, key, value);
        } else {
            log->logCreateFail(&memberNode->addr, isCoordinator, transID, key, value);
        }
    } else if(type == READ) {
        if(success) {
            log->logReadSuccess(&memberNode->addr, isCoordinator, transID, key, value);
        } else {
            log->logReadFail(&memberNode->addr, isCoordinator, transID, key);
        }
    } else if(type == UPDATE) {
        if(success) {
            log->logUpdateSuccess(&memberNode->addr, isCoordinator, transID, key, value);
        } else {
            log->logUpdateFail(&memberNode->addr, isCoordinator, transID, key, value);
        }
    } else if(type == DELETE) {
        if(success) {
            log->logDeleteSuccess(&memberNode->addr, isCoordinator, transID, key);
        } else {
            log->logDeleteFail(&memberNode->addr, isCoordinator, transID, key);
        }
    }
}
