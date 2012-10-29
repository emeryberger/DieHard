/* -*- C++ -*- */

#ifndef __QIH_MYQUEUE_H__
#define __QIH_MYQUEUE_H__

namespace QIH {
  template <typename Element>
  class MyQueue {
  protected:    
    class QueueContainer : public InternalHeapObject {
    private:
      Element data;
      QueueContainer *next, *prev;
      
    public:
      QueueContainer(Element d) : data(d), next(NULL), prev(NULL) {
	
      }
      
      QueueContainer(Element d, QueueContainer *p) : data(d), next(NULL), prev(p) {
	if (prev)
	  prev->next = this;
      }
      
      inline Element getData() {
	return data;
      }

      inline QueueContainer *getNext() {
	return next;
      }
      
      inline QueueContainer *getPrevious() {
	return prev;
      }
      
      inline bool isHead() {
	return prev == NULL;
      }
      
      inline bool isTail() {
	return next == NULL;
      }
      
      inline void setNext(QueueContainer *n) {
	next = n;
      }
      
      inline void setPrevious(QueueContainer *p) {
	prev = p;
      }
    };

  public:    
    MyQueue() : head(NULL), tail(NULL), count(0) {
    }
    
    inline int getSize() {
      return count;
    }
    
    inline Element peek() {
      if (head)
	return head->getData();
      else
	return Element();
    }
    
    inline Element pop() {
      
      Element res(0);
      
      if (head) {
	res = head->getData();
	
	QueueContainer *tmp = head;
	
	if (head->getNext()) {//not last element
	  head = head->getNext();
	  head->setPrevious(NULL);
	} else { //last element
	  head = tail = NULL;
	}
	
	delete tmp;
	
	count--;
	
      }
      
      return res;
    }
    
    inline void push(Element e) {
      if (tail) { //not the first element
	
	tail = new QueueContainer(e, tail);
	
	count++;
	
      } else { //first element
	head = new QueueContainer(e);
	tail = head;
	count = 1;
      }
    }
    
  protected:
    QueueContainer *head, *tail;
    
    int count;
  }; 
}
#endif
