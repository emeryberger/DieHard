/* -*- C++ -*- */

#ifndef __QIH_PAGEQUEUE_H__
#define __QIH_PAGEQUEUE_H__

#include "myqueue.h"
#include "pagedescription.h"

namespace QIH {

  
  class PageQueue : public MyQueue<PageDescription*>  {
  public:

    typedef MyQueue<PageDescription*>::QueueContainer PageContainer;

    inline PageDescription *pop() {
      PageDescription *r = MyQueue<PageDescription*>::pop();
      r->pos = NULL; //clear the position
      return r;
    }

    inline void push(PageDescription *p) {

      MyQueue<PageDescription*>::push(p);

      p->pos = static_cast<void*>(MyQueue<PageDescription*>::tail);
    }

    inline void removeObject(PageDescription *p) {
      PageContainer *c;

      while (p) { //walk down the chain of pages

	c = static_cast<PageContainer*>(p->pos); //HACK

	if (!c) {
	  p = p->next; //page not in the queue, go on
	  continue;
	}

	if (c == MyQueue<PageDescription*>::head) { //first element

	  MyQueue<PageDescription*>::head = c->getNext();

	  if (MyQueue<PageDescription*>::head == NULL) //also last element
	    MyQueue<PageDescription*>::tail = NULL;
	  else
	    MyQueue<PageDescription*>::head->setPrevious(NULL);
	  
	} else if (c == MyQueue<PageDescription*>::tail) { //last element

	  MyQueue<PageDescription*>::tail = c->getPrevious();
	  MyQueue<PageDescription*>::tail->setNext(NULL);

	} else { //element in the middle
	  c->getPrevious()->setNext(c->getNext());  //patch the pointers together
	  c->getNext()->setPrevious(c->getPrevious());
	}

	p->pos = NULL; //no longer in queue

	delete c; //delet the container

	MyQueue<PageDescription*>::count--;
	
	p = p->next; //next page
      }
    }
  };
}

#endif 
