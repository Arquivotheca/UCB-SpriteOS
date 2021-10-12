
// Here is the definition for the Stack class.  The functions are
// implemented in the file stack.cc.
//
// A stack requires one parameter in its constructor: the number of elements
// it can hold.  The element type is integer.

class Stack {
  public:
    Stack(int sz);    // Constructor:  initialize variables, allocate space.
    ~Stack();         // Destructor:   deallocate space allocated above.
    
    void Push(int i); // Push an integer, checking for overflow.
    int Pop();        // Pop an integer, checking for underflow.
    
    int Full();       // Returns non-0 if the stack is full, 0 otherwise.
    int Empty();      // Returns non-0 if the stack is empty, 0 otherwise.
    
  private:
    int size;         // The maximum capacity of the stack.
    int top;          // Index of the lowest unused position.
    int* stack;       // A pointer to an array that holds the contents.
};
