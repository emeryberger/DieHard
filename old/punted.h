  /*
  VALUE_TYPE * findOrInsert(KEY_TYPE key) {
    int begin = key & _mask;
    int lim = (begin - 1 + _map_size) & _mask;

    int probes = 0;

    for(int i = begin; i != lim; i = (i+1) & _mask) {
      probes++;
      if(probes == 10) {
	// XXX: fix hash function to lower clustering?
	//printf("probed a lot of times\n");
      }
      if(_entries[i].isValid()) {
	if(_entries[i].getHashCode() == key) {
	  return &_entries[i];
	} else { 
	  continue;
	}
      } else {
	if(_num_elts+1 > 0.5*_map_size) {
	  grow();
	  return findOrInsert(key);
	} else {
	  _num_elts++;
	  if(!(_num_elts % 100)) {
	    char buf[80];
	    sprintf(buf,"now %d active callsites\n",_num_elts);
	    //OutputDebugString(buf);
	  }
	  return new (&_entries[i]) VALUE_TYPE(key);
	}
      }
    }

    // path cannot be reached---must find empty bin since load factor < 1.0
    printf("failing to find key 0x%x\n",key);
    
    abort();
    return NULL;
  }
  */
