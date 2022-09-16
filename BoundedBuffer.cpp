#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    // 3. Then push the vector at the end of the queue
    // 4. Wake up threads that were waiting for push
    vector<char> pass(msg, msg + size);
    unique_lock<mutex> sx(m);
    a_slot.wait(sx, [this] { return (int)this->size() < cap; });
    q.push(pass);
    sx.unlock();
    a_data.notify_all(); 
}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    // 4. Wake up threads that were waiting for pop
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    unique_lock<mutex> sx(m);
    a_data.wait(sx, [this]{ return (int)this->size() >= 1; });
    vector<char> temp = q.front();
    q.pop();
    sx.unlock();
    a_slot.notify_all();
    char* popped = temp.data();
    int poppedSize = temp.size();
    if (poppedSize > size) {
        exit(-1);
    }
    memcpy(msg, popped, poppedSize);
    return poppedSize;
}

size_t BoundedBuffer::size () {
    return q.size();
}