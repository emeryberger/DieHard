#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// the STL vector class
#include <vector> 

// the STL list class
#include <list> 

//#define BINARY_TRACE	1

#define DO_ALL		1
//#define NUM_PHASES	4


using namespace std;

bool debug = false;
int num_errors = 0;

#if DO_ALL
vector<int> histogram;
list<size_t> LRU_queue;
#endif

list<size_t>::iterator iter;

#if NUM_PHASES
vector<int> phase_hist[NUM_PHASES];
list<size_t> phase_lru[NUM_PHASES];

int current_phase;
#endif

// calculate page address distance for prefetchable misses
size_t preset_memsize = 0;
FILE * cold_pa = NULL;


//----------------------------------------------------------------------//
// function to print LRU Queue
void 
printList (list<size_t> l) {

	list<size_t>::iterator iter;
	iter = l.begin();
	while (iter != l.end()) {

		printf("->%x", *iter);
		iter++;

	}
	printf("\n");

}

inline void LRUprocess(size_t page_no) {
	//search for the page in the LRU queue
	bool found_page = false;
	size_t page_pos = 0;

#if DO_ALL
	if (debug) printList(LRU_queue);

	// check if page is in LRU queue
	iter = LRU_queue.begin();
	while (iter != LRU_queue.end()) {
		if (*iter == page_no) {
			found_page = true;
			break;
		}
		iter++;
		page_pos++;
	}

	if (found_page) {
		// page is in the LRU queue, locate it and increase histogram
		if (debug) printf("page found at position %i.", page_pos);
      
		// move page to beginning not needed if page is at the beginning
		if (page_pos != 0) {

			if (debug) printf(" moving page to front\n");

			// remove from the middle of the Queue
			LRU_queue.erase(iter);

			// put at the beginning of the list
			LRU_queue.push_front(page_no);

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
		// page is not in queue, load it to the "last loaded" spot
		if (debug) printf("page not found - is loaded\n");
		LRU_queue.push_front(page_no);
	}
#endif

#if NUM_PHASES
	found_page = false;
	page_pos = 0;

	iter = phase_lru[current_phase].begin();
	while (iter != phase_lru[current_phase].end()) {
		if (*iter == page_no) {
			found_page = true;
			break;
		}
		iter++;
		page_pos++;
	}

	if (found_page) {
		// page is in the LRU queue, locate it and increase histogram
		if (debug) printf("page found at position %i.", page_pos);
      
		// move page to beginning not needed if page is at the beginning
		if (page_pos != 0) {

			if (debug) printf(" moving page to front\n");

			// remove from the middle of the Queue
			phase_lru[current_phase].erase(iter);

			// put at the beginning of the list
			phase_lru[current_phase].push_front(page_no);

		}
		else {
			if (debug) printf("\n");
		}

		// if the number of locations in histogram is smaller than 
		// the current hit position, add elements to vector
		if (phase_hist[current_phase].size() <= page_pos) {

			while (phase_hist[current_phase].size() < page_pos) {
				phase_hist[current_phase].push_back(0);
			}
			phase_hist[current_phase].push_back(1);
		}
		else {
			phase_hist[current_phase][page_pos]++;
		}
	}
	else {
		// page is not in queue, load it to the "last loaded" spot
		if (debug) printf("page not found - is loaded\n");
		phase_lru[current_phase].push_front(page_no);
	}
#endif
}

inline void NewLRUprocess(char tag, size_t page_no) {
	// search for the page in the LRU queue
	bool found_page = false;
	size_t page_pos = 0;
	list<size_t>::iterator placeholder = NULL;

	if (debug) printList(LRU_queue);

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
	if (tag == 'S' || tag == 'M' || tag == 'd') {	// first access of anonymous page
		if (found_page) {	// this should not happen
			printf("OOPS! error #%d: page receiving first access should not be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);
//			return;			// to ignore a potential error in trace (seems to be fixed now)
			exit(1);
		}

		// this is a zero-on-demand page, we put it at the front of LRU queue but do not increment histogram
		LRU_queue.push_front(page_no);

		// fill place holder if any
		if (placeholder != NULL) {
			LRU_queue.erase(placeholder);
		}
	}
	else if (tag == 's' || tag == 'm') {	// repeated access into sbrked/mmaped page
		if (!found_page) {	// this should not happen
			printf("error #%d: page being accessed should be in LRU (tag=%c page=%p)\n", ++num_errors, tag, page_no);
			exit(1);
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
	else if (tag == 'U' || tag == 'D') {		// sbrked/mmaped page unmapped or discarded
		if (!found_page) {	// this should not happen (actually this could happen because the page can be unmapped/discarded before its first access)
			printf("warning: page being unmapped/discarded should be in LRU (tag=%c page=%p)\n", tag, page_no);
			return;
//			exit(1);
		}

		*iter = 0;	// set this position as place holder
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
		printf("usage: %s reference_trace_file base_of_output_files [preset_memsize]\n", argv[0]);
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
	if (rc != 0) {
		printf("stat() returns %d\n", rc);
		exit(1);
	}
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

#if BINARY_TRACE
	size_t n = fread(&page_num, sizeof(size_t), 1, trace_file);
	while (n == 1) {
		line_num++;

		tag_char = (char) (page_num & 0xff);
		page_num &= ~0xff;

		if (page_num == 0 || (page_num & 0x3ff) != 0) {
			printf("error #%d: page %p not aligned at line %d\n", ++num_errors, page_num, line_num);
			exit(1);
		}

		NewLRUprocess(tag_char, page_num);

		if (line_num % 100000 == 0) {
			long bytes_read = ftell(trace_file);
#if NUM_PHASES
			printf("phase %d, %u bytes read, %d lines processed, %.0f%% finished\n", current_phase + 1, bytes_read, line_num, (double)bytes_read * 100 / file_size);
#else
			printf("%u bytes read, %d lines processed, %.0f%% finished\n", bytes_read, line_num, (double)bytes_read * 100 / file_size);
#endif
		}

		n = fread(&page_num, sizeof(size_t), 1, trace_file);
	}
#else	// BINARY_TRACE
	char line[500];
	char * s = fgets(line, sizeof(line), trace_file);

#if NUM_PHASES
	current_phase = 0;
#endif

	// repeat while there is more of the trace
	while (s != NULL) {
		line_num++;
#if 0	// kVMTrace format
		int rc = sscanf(line, "%c %x %x %x", &tag_char, &junk_int1, &junk_int2, &page_num);
		//printf("%c %x %x %x\n", tag_char, junk_int1, junk_int2, page_num);

		if (rc != 4) {
			printf("error in converting input, scanf returns %d at line %d\n", rc, line_num);
			exit(1);
		}

		if (page_num == 0 || (page_num & 0x3ff) != 0) {
			printf("error #%d: page %p not aligned at line %d\n", ++num_errors, page_num, line_num);
			exit(1);
		}

		switch (tag_char) {
		case 'r':
		case 'R':
		case 'w':
		case 'W':
			LRUprocess(page_num);
			break;
		default:
			break;
		}
#else	// memusage.h format
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

		NewLRUprocess(tag_char, page_num);
#endif

		if (line_num % 100000 == 0) {
			long bytes_read = ftell(trace_file);
#if NUM_PHASES
			printf("phase %d, %u bytes read, %d lines processed, %.0f%% finished\n", current_phase + 1, bytes_read, line_num, (double)bytes_read * 100 / file_size);
#else
			printf("%u bytes read, %d lines processed, %.0f%% finished\n", bytes_read, line_num, (double)bytes_read * 100 / file_size);
#endif
		}

#if NUM_PHASES
		if (line_num % 10000 == 0) {
			if ((double)ftell(trace_file) / file_size >= (double)(current_phase + 1) / NUM_PHASES) {
				current_phase++;
				if (current_phase >= NUM_PHASES) {
					printf("error current_phase=%d\n", current_phase);
				}
			}
		}
#endif

		s = fgets(line, sizeof(line), trace_file);
	}

	printf("processed %d lines from input\n", line_num);

#endif	// BINARY_TRACE


	char hist_filename[30];
	char miss_filename[30];

	// file to output miss counts
	FILE * hist_file;
	FILE * miss_file;

	// CALCULATE MISSES //

#if DO_ALL
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

	fflush(cold_pa);
	fclose(cold_pa);
#endif

#if NUM_PHASES
	// create the misses array
	size_t max_size = phase_hist[0].size();

	for (int i = 1; i < NUM_PHASES; i++) {
		if (phase_hist[i].size() > max_size) {
			max_size = phase_hist[i].size();
		}
	}

	unsigned int phase_misses[max_size];

	for (current_phase = 0; current_phase < NUM_PHASES; current_phase++) {

		phase_misses[phase_hist[current_phase].size()-1] = phase_hist[current_phase][phase_hist[current_phase].size()-1];
		for (int i = phase_hist[current_phase].size() - 2; i >= 0; i--) {
			// at each page number, total misses are 
			//                 number of hits + all misses at next page
			phase_misses[i] = phase_misses[i+1] + phase_hist[current_phase][i];
		}
   
		//----------------------------------------------------------------------//
		// OUTPUT TO FILES //
  
		// figure out output file names
		sprintf(hist_filename, "%s.hist.%d", base_name, current_phase + 1);
		sprintf(miss_filename, "%s.miss.%d", base_name, current_phase + 1);

		// open files for writing
		miss_file = fopen(miss_filename, "w");
		hist_file = fopen(hist_filename, "w");

		// output miss count to MISS_OUTPUT
		fprintf(miss_file, "#miss count vs memory size (num of pages):\n");
		fprintf(miss_file, "#memsize  misses\n");

		// output histogram to stdout
		fprintf(hist_file, "#histogram of LRU simulation :\n");
		fprintf(hist_file, "#page_position number_of_references\n");

		for(int i = 0; i < phase_hist[current_phase].size(); i++) {
			fprintf(hist_file, "%8i %10i\n",i+1, phase_hist[current_phase][i]);
			fprintf(miss_file, "%8i %10i\n", i+1, phase_misses[i]);
		}

		fflush(hist_file);
		fflush(miss_file);

		fclose(hist_file);
		fclose(miss_file);
	}
#endif

	return 0;
} 
