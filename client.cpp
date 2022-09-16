#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"
#include "signal.h"

// ecgno to use for datamsgs
#define ECGNO 1

using namespace std;

BoundedBuffer* info_messages;
BoundedBuffer* error_messages;

void infoSigHandler(int signalNum) {
    // Print info statements to cout until the info_messages queue is empty.
    // For each statement, cout "INFO: " + message
    char info[2048]; // Buffer to hold the info message
    while (info_messages->size() > 0) {
    //SIGNAL_TYPE _stype, char* _msg_ptr, int _msg_size
        //To create the info message
        sigmsg* inMsg = (sigmsg*) info;
        //popping the message into info
        int poppedMessage = info_messages->pop(info, 2048);
        //to be able to print it out through memory copy
        //memcpy(&inMsg, info, poppedMessage);
        //cout it
        cout << "INFO: " << inMsg->msg << endl;
    }
}

void errorSigHandler(int signalNum) {
    // Print error statements to cerr until the info_messages queue is empty.
    // For each statement, cout "ERROR: " + message
    char error[2048]; // Buffer to hold the error message
    while (error_messages->size() > 0) {
        //SIGNAL_TYPE _stype, char* _msg_ptr, int _msg_size
        //To create the error message
        sigmsg* errMsg = (sigmsg*) error;
        //popping the message into error
        int poppedMessage = error_messages->pop(error, 2048);
        //to be able to print it out through memory copy
        //memcpy(&errMsg, error, poppedMessage);
        //cout it
        cout << "ERROR: " << errMsg->msg << endl;
    }
}

void signal_thread_function (FIFORequestChannel* chan) {
    // Initialize the info and error bounded buffers
    info_messages = new BoundedBuffer(10);
    error_messages = new BoundedBuffer(10);

    // Set the signal handlers here with signal
    // Initialize the info and error bounded buffers
    // Info is SIGUSR1
    // Error is SIGUSR2
    signal(SIGUSR1, infoSigHandler);
    signal(SIGUSR2, errorSigHandler);

    // Create a buffer to hold the message from the control channel
    char buf[2048];

    while(true) {
        // Read from the control channel
        // Check if the channel has been quit. If it has, break out of loop
        // Hint: look at number of bytes returned.
        int reply = chan->cread(buf, 2048);
        //If the number of bytes is negative it should break
        if (reply <= 0) {
            break;
        }
        // Check the signal type
        //create a new signal message
        sigmsg* sm = (sigmsg*) buf;
        //copy that into the signal message so it can be pushed
        //memcpy(&sm, buf, 512);
        // Push the message onto the info_messages queue
        if (sm->stype == SIGNAL_TYPE::INFO) {
            cout << "here" << endl;
            info_messages->push(buf, 2048);
            raise(SIGUSR1);
        }
        else {
            error_messages->push(buf, 2048);
            raise(SIGUSR2);
        }
        // Send the appropriate signal
    }

    // Clean up when done
    delete info_messages;
    delete error_messages;
    //delete[] buf;
}

void patient_thread_function (/* add necessary arguments */BoundedBuffer* requetBuff, int p_no, int num_req, int size) {
    // functionality of the patient threads
    //really similar to pa1
    double t = 0;
    for (int s = 0; s < num_req; s++) {
        char buf[size];
        datamsg x(p_no, t, ECGNO);
        memcpy(buf, &x, sizeof(datamsg));
        requetBuff->push(buf, sizeof(datamsg));
        t = t + 0.004;
    }
}

void file_thread_function (/* add necessary arguments */string filename, FIFORequestChannel* chan, int m, BoundedBuffer* requestBuff) {
    // functionality of the file thread
    filemsg fm(0, 0);
	string fname = filename;
	
	int len = sizeof(filemsg) + (fname.size() + 1);
	char* buf2 = new char[len];
	memcpy(buf2, &fm, sizeof(filemsg));
	strcpy(buf2 + sizeof(filemsg), fname.c_str());
	chan->cwrite(buf2, len);  // I want the file length;

	int64_t filesize = 0;
	//chan->cread(&filesize, sizeof(int64_t));
    if (chan->cread(&filesize, sizeof(int64_t)) < sizeof(__int64_t)) {
        perror("cread zero bytes");
        delete[] buf2;
        return;
    }

    //char* buf3 = new char[m];
    FILE * pfile = fopen(("received/" + fname).c_str(), "w");
    if (pfile == nullptr) {
        perror("file not found");
        delete[] buf2;
        return;
    }
    fseek(pfile, filesize, SEEK_SET);
	//ofstream foutput; //the output
	//string name = "received/" + filename; //string name
	//foutput.open (name); //open up the desired file
	for (int s = 0; s <= filesize/m; s++) { //section it off by the m 
		filemsg* file_req = (filemsg*)buf2; //request is the same as buffer
		file_req->offset = s * m; //offset
		if (s == filesize/m) {
            if (filesize - s * m == 0) {
                break;
            }
			file_req->length = filesize - s * m; //length
		}
		else {
			file_req->length =  m; //length at the very end
		}
		//push buf 2 into request
        requestBuff->push(buf2, len);
	} 
    fclose(pfile);

	//foutput.close(); //close the file

	delete[] buf2; //delete buffer number dos
}

void worker_thread_function (/* add necessary arguments */FIFORequestChannel* chan, BoundedBuffer* requestBuff, BoundedBuffer* responseBuff, int size) {
    // functionality of the worker threads
    for (;;) {
        char poppedMessage[size];
        int length = requestBuff->pop(poppedMessage, size);
        MESSAGE_TYPE m = *((MESSAGE_TYPE*) poppedMessage);
        /*if (length < 0) {
            if (m != SIGNAL_MSG) {
                continue;
            }
        }*/
	    if (m == DATA_MSG) {
            double reply;
            chan->cwrite(poppedMessage, sizeof(datamsg));
            if (chan->cread(&reply, sizeof(double)) < 0) {
                perror("bytes less than zero");
                return;
            }
            pair<int, double> p(((datamsg*)poppedMessage)->person, reply);
            responseBuff->push((char*)&p, sizeof(pair<int, double>));
        }
        else if (m == FILE_MSG) {
            filemsg* f;
            f = (filemsg*) poppedMessage;
            char* filename = (char*)(f+1);
            string fname = "received/" + string(filename);
            char* buf = new char[size];
            chan->cwrite(poppedMessage, sizeof(filemsg) + length);
            chan->cread(buf, size);
            FILE * pfile = fopen(fname.c_str(), "r+");
            if (pfile) {
                fseek(pfile, f->offset, SEEK_SET);
                fwrite(buf, 1, f->length, pfile);
            }
            fclose(pfile);
            delete[] buf;
        }
        else if (m == QUIT_MSG or m == UNKNOWN_MSG) {
            cout << "here" << endl;
            //write to channel to quit
            MESSAGE_TYPE m = QUIT_MSG;
            chan->cwrite ((char *) m, sizeof (QUIT_MSG));
            break;
        }
        
    }
}

void histogram_thread_function (/* add necessary arguments */BoundedBuffer* responseBuff, int size, HistogramCollection* hc) {
    // functionality of the histogram threads
    for (;;) {
        char poppedMessage[256];
        responseBuff->pop(poppedMessage, 256);
        pair<int, double>* p = (pair<int, double>*) poppedMessage;
        if (p->first == -1 and p->second == -1) {
            break;
        }
        hc->update(p->first, p->second);
    }
}

int main (int argc, char *argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 30;		// default capacity of the request buffer
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    vector<FIFORequestChannel*> channels;
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
		}
	}
    bool filereq = (f != "");
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	// control overhead (including the control channel)
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    int pSize = 0;
    int hSize = 0;
    if (f == "") {
        pSize = p;
        hSize = h;
    }
    else {
        pSize = 1;
        hSize = 1;
    }

    thread producerT[pSize];
    thread workerT[w];
    thread histogramT[hSize];

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    // Create the signal thread
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    
    if (f == "") {
        //creating paitent threads
        //creating worker threads
        for (int i = 0; i < w; i++) {
            //utilized pa1 creating channels
            char n[256];
            MESSAGE_TYPE nc = NEWCHANNEL_MSG;
            chan->cwrite(&nc, sizeof(MESSAGE_TYPE)); //varible to hold the name
            chan->cread(n, 256);
            FIFORequestChannel* newChan = new FIFORequestChannel(n, FIFORequestChannel::CLIENT_SIDE);
            channels.push_back(newChan);
            //FIFORequestChannel* chan, BoundedBuffer* requestBuff, BoundedBuffer* responseBuff, int size
            workerT[i] = thread(worker_thread_function, newChan, &request_buffer, &response_buffer, m);
        }
        for (int i = 0; i < pSize; i++) {
            //BoundedBuffer* requetBuff, int p_no, int num_req, int size
            producerT[i] = thread(patient_thread_function, &request_buffer, i + 1, n, m);
        }
        //creating histogram threads
        for (int i = 0; i < hSize; i++) {
            //BoundedBuffer* responseBuff, int size, HistogramCollection* hc
            histogramT[i] = thread(histogram_thread_function, &response_buffer, m, &hc);
        }
    }
    else {
        //creating worker threads
        for (int i = 0; i < w; i++) {
            //utilized pa1 creating channels
            char n[256];
            MESSAGE_TYPE nc = NEWCHANNEL_MSG;
            chan->cwrite(&nc, sizeof(MESSAGE_TYPE)); //varible to hold the name
            chan->cread(n, 256);
            FIFORequestChannel* newChan = new FIFORequestChannel(n, FIFORequestChannel::CLIENT_SIDE);
            channels.push_back(newChan);
            //FIFORequestChannel* chan, BoundedBuffer* requestBuff, BoundedBuffer* responseBuff, int size
            workerT[i] = thread(worker_thread_function, newChan, &request_buffer, &response_buffer, m);
        }
        char n[256];
        MESSAGE_TYPE nc = NEWCHANNEL_MSG;
        chan->cwrite(&nc, sizeof(MESSAGE_TYPE)); //varible to hold the name
        chan->cread(n, 256);
        FIFORequestChannel* newChan = new FIFORequestChannel(n, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(newChan);
        //string filename, FIFORequestChannel* chan, int m, BoundedBuffer* requestBuff
        producerT[0] = thread(file_thread_function, f, newChan, m, &request_buffer);
    }

    thread signal_thread(signal_thread_function, chan);

	/* join all threads here */
    for (int i = 0; i < pSize; i++) {
        producerT[i].join();
    }
    for (int i = 0; i < w; i++) {
        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push((char*)&q, sizeof(q));
    }
    for (int i = 0; i < w; i++) {
        workerT[i].join();
    }
    for (int i = 0; i < hSize; i++) {
        //MESSAGE_TYPE q = QUIT_MSG;
        pair<int, double> q(-1, -1);
        response_buffer.push((char*)&q, sizeof(q));
    }
    if (f == "") {
        for (int i = 0; i < hSize; i++) {
            histogramT[i].join();
        }
    }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
    
	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    for (int a = 0; a < (int)channels.size(); a++) {
        delete channels.at(a);
	}
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    signal_thread.join();
    delete chan;

	// wait for server to exit
	wait(nullptr);
}