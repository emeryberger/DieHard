#include <queue>
using namespace std;

queue<worklist_item> worklist;

void enqueue_worklist_item(const worklist_item & item) {
  worklist.push(item);
}

worklist_item next_worklist_item() {
  worklist_item foo = worklist.top();
  worklist.pop();
  return foo;
}

void process_worklist() {
  while(!worklist.empty()) {
    worklist_item next = next_worklist_item();

    check_overflow(next);
  }
}
