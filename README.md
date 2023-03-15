# Cloud-Computing-Projects
Final Projects related to Coursera Courses: Cloud Computing Concepts-1 and Cloud Computing Concepts-2 offered by
University of Illinois

## Machine Programming Assignment - 1 (mp1)
### Membership Protocol
In this assignment, the Gossip Membership Protocol is implemented. Other membership protocol include all-to-all heartbeating, and SWIM-style membership.

### Features of the Implementation:
1) **Introduction:** Each new peer contacts a well-known peer (the introducer) to join the group.
2) **Membership:** Every second, each peer contacts some random peers from it's memnbership list and pings a heartbeat for a response with it's membership list, thus updating it's own membership list with the response. If it does not get a response till a threshold seconds, it marks the peer as failed.
3) **Completeness all the time:** The protocol satisfies completeness. Thus, every non-faulty process detects every node join, failure, and leave, 
4) **Accuracy of failure detection:** When there are no message losses and message delays are small, the failures are detected with full accuracy. When there are message losses, completeness is satisfied and accuracy is high even with simultaneous multiple failures.

## Machine Programming Assignment - 2 (mp2)
### Fault-Tolerant Key-Value Store
In this assignment, a ring based distributed key-value store is implemented.

### Features of the Implementation:
1) **Membership:** Supports the gossip protocol implemented in mp1.
2) **Key-value store:** Supports CRUD operations (Create, Read, Update, Delete).
3) **Load-balancing:** Used consistent hashing ring to hash both servers and keys.
4) **Fault-tolerance:** The system is fault-tolerant up to two failures by replicating each key three times to three successive nodes in the ring, starting from the first node at the hashed key and to 2 clockwise neighbours.
5) **Quorum:** with consistency level of 2 for both reads and writes.
6) **Stabilization:** Automatic replication to three nodes after a node is failed.

## References
1) Cloud Computing Concepts - 1
   https://www.coursera.org/learn/cloud-computing
2) Cloud Computing Concepts - 2
   https://www.coursera.org/learn/cloud-computing-2
