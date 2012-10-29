#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// the STL vector class
#include <vector> 

// the STL list class
#include <list> 


using namespace std;

bool debug = false;
int num_errors = 0;

vector<int> histogram;
list<size_t> LRU_queue;

list<size_t>::iterator iter;

// calculate page address distance for prefetchable misses
size_t preset_memsize = 0;
FILE * cold_pa = NULL;


inline void LRUprocess(char tag, size_t page_no) {
	// search for the page in the LRU queue
	bool found_page = false;
	size_t page_pos = 0;
	list<size_t>::iterator placeholder = NULL;

	// check if page is in LRU queue and find first place holder if any
	iter = LRU_queue.begin();
	while (iter != LRU_queue.end()) {
		if (placeholder == NULL && *iter == 0) {	// found place holder
			placeholder = iter;
		}
		else if (*iter == page_no) {
			found_page = true;
			break;
		}
		iter++;
		page_pos++;
	}

#if 1
	if (tag == 'A' || tag == 'P' || tag == 'S') {	// memory map operations
		if (found_page) {	// this should not happen
			printf("OOPS! error #%d: page receiving first access should not be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);
//			return;			// to ignore a potential error in trace (seems to be fixed now)
			exit(1);
		}
	}
	else if (tag == 'U' || tag == 'D') {		// memory unmap or discard operations
		if (!found_page) {
//			printf("warning: page being unmapped/discarded should be in LRU (tag=%c page=%p)\n", tag, page_no);
			return;
		}

		*iter = 0;	// set this position as place holder
	}
	else if (tag == 'R') {					// first touch on a zero-on-demand page
		if (found_page) {	// this should not happen
			printf("OOPS! error #%d: page receiving first access should not be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);
//			exit(1);

			*iter = 0;
		}

		// this is a zero-on-demand page, we put it at the front of LRU queue but do not increment histogram
		LRU_queue.push_front(page_no);

		// fill place holder if any
		if (placeholder != NULL) {
			LRU_queue.erase(placeholder);
		}
	}
	else if (tag == 'r' || tag == 'w') {	// repeated access into sbrked/mmaped page
		if (!found_page) {	// this should not happen
			printf("error #%d: page being accessed should be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);

			LRU_queue.push_front(page_no);

			// fill place holder if any
			if (placeholder != NULL) {
				LRU_queue.erase(placeholder);
			}

			return;
		}

		// page is in the LRU queue, locate it and increase histogram
		if (debug) printf("page found at position %i.", page_pos);
      
		// move page to beginning not needed if page is at the beginning
		if (page_pos != 0) {
			if (debug) printf(" moving page to front\n");

			// put it at the front of LRU queue
			LRU_queue.push_front(page_no);


			if (placeholder == NULL) {
				// remove from the old position
				LRU_queue.erase(iter);
			}
			else {
				// trickle down place holder
				LRU_queue.erase(placeholder);
				*iter = 0;	// set old position as place holder
			}
		}
		else {
			if (debug) printf("\n");
		}

		// if the number of locations in histogram is smaller than 
		// the current hit position, add elements to vector
		if (histogram.size() <= page_pos) {

			while (histogram.size() < page_pos) {
				histogram.push_back(0);
			}
			histogram.push_back(1);
		}
		else {
			histogram[page_pos]++;
		}

		// output the page no if it is a miss for the preset mem size
		if (preset_memsize != 0 && page_pos >= preset_memsize) {
			fprintf(cold_pa, "%x\n", page_no);
		}
	}
	else {
		printf("error #%d: unrecognized tag (tag=%c page=%p)\n", ++num_errors, tag, page_no);
		exit(1);
	}

#else	// ignore page unmapping and discarding

	if (tag == 'S' || tag == 'M' || tag == 'd' || tag == 's' || tag == 'm') {
		if (found_page) {
			// move page to beginning not needed if page is at the beginning
			if (page_pos != 0) {
				if (debug) printf(" moving page to front\n");

				// put it at the front of LRU queue
				LRU_queue.push_front(page_no);

				// remove from the old position
				LRU_queue.erase(iter);
			}
			else {
				if (debug) printf("\n");
			}

			// if the number of locations in histogram is smaller than 
			// the current hit position, add elements to vector
			if (histogram.size() <= page_pos) {

				while (histogram.size() < page_pos) {
					histogram.push_back(0);
				}
				histogram.push_back(1);
			}
			else {
				histogram[page_pos]++;
			}
		}
		else {
			if (tag != 'S' && tag != 'M') {
				printf("error #%d: page being accessed should be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);
				exit(1);
			}

			// this is a zero-on-demand page, we put it at the front of LRU queue but do not increment histogram
			LRU_queue.push_front(page_no);
		}
	}
	else if (tag == 'U' || tag == 'D') {		// sbrked/mmaped page unmapped or discarded
		if (!found_page) {	// this should not happen (actually this could happen because the page can be unmapped/discarded before its first access)
			printf("warning: page being unmapped/discarded should be in LRU (tag=%c page=%p)\n", tag, page_no);
			return;
//			exit(1);
		}
	}
	else {
		printf("error #%d: unrecognized tag (tag=%c page=%p)\n", ++num_errors, tag, page_no);
		exit(1);
	}
#endif
}

int main(int argc, const char* argv[]) {
	if (argc < 3 || argc > 4) {
		printf("usage: %s memtrace_file output_base_name [preset_memsize]\n", argv[0]);
		exit(1);
	}

	char * trace_name = (char *)argv[1];
	char * base_name = (char *)argv[2];
	if (argc == 4) {
		preset_memsize = atoi(argv[3]);

		assert(preset_memsize % 256 == 0);

		char cold_pa_filename[20];
		strcpy(cold_pa_filename, base_name);
		strcat(cold_pa_filename, ".pa");
		strcat(cold_pa_filename, argv[3]);

		cold_pa = fopen(cold_pa_filename, "w");
		if (cold_pa == NULL) {
			exit(1);
		}
	}
	else {
		preset_memsize = 0;
	}

	struct stat stat_buf;
	int rc = stat(trace_name, &stat_buf);
#if 0
	if (rc != 0) {
	  perror ("Error in stat!");
	  exit(1);
	}
#endif
	long file_size = stat_buf.st_size;
	printf("%s has %u bytes\n", trace_name, file_size);

	FILE * trace_file = fopen(trace_name, "r");
	if (trace_file == NULL) {
		printf("fopen() returns error\n");
	}

	char tag_char;
	int junk_int1;
	int junk_int2;
	size_t page_num;

	int line_num = 0;

	char line[500];
	char * s = fgets(line, sizeof(line), trace_file);

	// repeat while there is more of the trace
	while (s != NULL) {
		line_num++;

		int rc = sscanf(line, "%c %x", &tag_char, &page_num);
		//printf("%x\n", page_num);

		if (rc != 2) {
			printf("error #%d: in converting input, scanf returns %d at line %d\n", ++num_errors, rc, line_num);
			exit(1);
		}

		if (page_num == 0 || (page_num & 0x3ff) != 0) {
			printf("error #%d: page %p not aligned at line %d\n", ++num_errors, page_num, line_num);
			exit(1);
		}

		LRUprocess(tag_char, page_num);

		if (line_num % 100000 == 0) {
			long bytes_read = ftell(trace_file);
			printf("%u bytes read, %d lines processed, %.0f%% finished\n", bytes_read, line_num, (double)bytes_read * 100 / file_size);
		}

		s = fgets(line, sizeof(line), trace_file);
	}

	printf("processed %d lines from input\n", line_num);


	char hist_filename[30];
	char miss_filename[30];

	// file to output miss counts
	FILE * hist_file;
	FILE * miss_file;

	// CALCULATE MISSES //

	// create the misses array
	unsigned int misses[histogram.size()];

	// the last element is equal to the last histogram entry
	misses[histogram.size()-1] = histogram[histogram.size()-1];
	for (int i = histogram.size() - 2; i >= 0; i--) {
		// at each page number, total misses are 
		//                 number of hits + all misses at next page
		misses[i] = misses[i+1] + histogram[i];
	}
   
	//----------------------------------------------------------------------//
	// OUTPUT TO FILES //
  
	// figure out output file names

	strcpy(hist_filename, base_name);
	strcat(hist_filename,".hist");
	strcpy(miss_filename, base_name);
	strcat(miss_filename,".miss");

	// open files for writing
	miss_file = fopen(miss_filename, "w");
	hist_file = fopen(hist_filename, "w");

	// output miss count to MISS_OUTPUT
	fprintf(miss_file, "#miss count vs memory size (num of pages):\n");
	fprintf(miss_file, "#memsize  misses\n");

	// output histogram to stdout
	fprintf(hist_file, "#histogram of LRU simulation :\n");
	fprintf(hist_file, "#page_position number_of_references\n");

	for(int i = 0; i < histogram.size(); i++) {
		fprintf(hist_file, "%8i %10i\n",i+1, histogram[i]);
		fprintf(miss_file, "%8i %10i\n", i+1, misses[i]);
	}

	fflush(hist_file);
	fflush(miss_file);

	fclose(hist_file);
	fclose(miss_file);

	if (cold_pa != NULL) {
		fflush(cold_pa);
		fclose(cold_pa);
	}

	return 0;
} 
