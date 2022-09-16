#include <thread>
#include "FIFORequestChannel.h"

using namespace std;

int buffercapacity = MAX_MESSAGE;
char* buffer = NULL; // buffer used by the server, allocated in the main
FIFORequestChannel* control_channel;
bool sent_invalid_person = false;

int nchannels = 0;
vector<string> all_data[NUM_PERSONS];


// pre-declared because function signature required call in process_newchannel_request
void handle_process_loop (FIFORequestChannel* _channel);
void send_signal_msg(SIGNAL_TYPE stype, string msg);

void process_newchannel_request (FIFORequestChannel* _channel) {
	nchannels++;
	string new_channel_name = "data" + to_string(nchannels) + "_";
	char buf[30];
	strcpy(buf, new_channel_name.c_str());
	_channel->cwrite(buf, new_channel_name.size()+1);

	FIFORequestChannel* data_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::SERVER_SIDE);
	thread thread_for_client(handle_process_loop, data_channel);
	thread_for_client.detach();
}


void populate_file_data (int person) {
	//cout << "populating for person " << person << endl;
	string filename = "BIMDC/" + to_string(person) + ".csv";
	char line[100];
	ifstream ifs(filename.c_str());
	if (ifs.fail()){
		EXITONERROR("Data file: " + filename + " does not exist in the BIMDC/ directory");
	}
	
	while (!ifs.eof()) {
		line[0] = 0;
		ifs.getline(line, 100);
		if (ifs.eof()) {
			break;
		}
		
		if (line[0]) {
			all_data[person-1].push_back(string(line));
		}
	}
}

double get_data_from_memory (int person, double seconds, int ecgno) {
	int index = (int) round(seconds / 0.004);
	string line = all_data[person-1][index]; 
	vector<string> parts = split(line, ',');
	
	double ecg1 = stod(parts[1]);
	double ecg2 = stod(parts[2]); 
	if (ecgno == 1) {
		return ecg1;
	}
	else {
		return ecg2;
	}
}

void process_file_request (FIFORequestChannel* rc, char* request) {
	MESSAGE_TYPE unkown = MESSAGE_TYPE::UNKNOWN_MSG;
	filemsg f = *((filemsg*) request);
	string filename = request + sizeof(filemsg);
	filename = "BIMDC/" + filename; // adding the path prefix to the requested file name
	//cout << "Server received request for file " << filename << endl;

	/* request buffer can be used for response buffer, because everything necessary have
	been copied over to filemsg f and filename*/
	char* response = request; 

	// make sure that client is not requesting too big a chunk
	if (f.length > buffercapacity) {
		send_signal_msg(SIGNAL_TYPE::ERROR, "Client is requesting a chunk bigger than server's capacity.");
		rc->cwrite(&unkown, sizeof(MESSAGE_TYPE));
	}

	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp) {
		string err_msg = "File cannot be opened. File: ";
		send_signal_msg(SIGNAL_TYPE::ERROR, err_msg.append(filename));
		rc->cwrite(&unkown, sizeof(MESSAGE_TYPE));
		return;
	}
	fseek(fp, f.offset, SEEK_SET);
	int nbytes = fread(response, 1, f.length, fp);

	if (f.offset == 0 && f.length == 0) { // means that the client is asking for file size
		__int64_t fs = get_file_size (filename);
		rc->cwrite ((char*) &fs, sizeof(__int64_t));
		return;
	}

	/* making sure that the client is asking for the right # of bytes,
	this is especially imp for the last chunk of a file when the 
	remaining lenght is < buffercap of the client*/
	assert(nbytes == f.length); 

	__int64_t fs = get_file_size (filename);
	string info_msg = "";
	if((f.offset <= fs/4) && (f.offset + f.length > fs/4)) {
		info_msg = "Quarter through file.";
	}
	else if((f.offset <= fs/2) && (f.offset + f.length > fs/2)) {
		info_msg = "Halfway through file.";
	}
	else if((f.offset <= 3*fs/4) && (f.offset + f.length > 3*fs/4)) {
		info_msg = "Three quarters through file.";
	}
	send_signal_msg(SIGNAL_TYPE::INFO, info_msg);

	rc->cwrite(response, nbytes);
	fclose(fp);
}

void process_data_request (FIFORequestChannel* rc, char* request) {
	MESSAGE_TYPE unkown = MESSAGE_TYPE::UNKNOWN_MSG;
	datamsg* d = (datamsg*) request;
	if((d->person < 1) || (d->person > NUM_PERSONS) || (d->ecgno < 1) || (d->ecgno > 2)) {
		if(!sent_invalid_person) {
			string err_msg = "For datamsg, the person number or ecg value is not correct.";
			send_signal_msg(SIGNAL_TYPE::ERROR, err_msg);
			sent_invalid_person = true;
		}
		rc->cwrite(&unkown, sizeof(MESSAGE_TYPE));
		return;
	}

	double data = get_data_from_memory(d->person, d->seconds, d->ecgno);
	rc->cwrite(&data, sizeof(double));
}

void process_unknown_request (FIFORequestChannel* rc) {
	char a = 0;
	rc->cwrite(&a, sizeof(char));
}

void process_request (FIFORequestChannel* rc, char* _request) {
	MESSAGE_TYPE m = *((MESSAGE_TYPE*) _request);
	if (m == DATA_MSG) {
		usleep(rand() % 5000);
		process_data_request(rc, _request);
	}
	else if (m == FILE_MSG) {
		process_file_request(rc, _request);
	}
	else if (m == NEWCHANNEL_MSG) {
		process_newchannel_request(rc);
	}
	else {
		process_unknown_request(rc);
	}
}

void handle_process_loop (FIFORequestChannel* channel) {
	/* creating a buffer per client to process incoming requests
	and prepare a response */
	char* buffer = new char[buffercapacity];
	if (!buffer) {
		EXITONERROR ("Cannot allocate memory for server buffer");
	}

	while (true) {
		int nbytes = channel->cread(buffer, buffercapacity);
		if (nbytes < 0) {
			send_signal_msg(SIGNAL_TYPE::ERROR, "Client-side terminated abnormally.");
			break;
		}
		else if (nbytes == 0) {
			send_signal_msg(SIGNAL_TYPE::ERROR, "Server received empty buffer.");
			break;
		}

		MESSAGE_TYPE m = *((MESSAGE_TYPE*) buffer);
		if (m == QUIT_MSG) {  // note that QUIT_MSG does not get a reply from the server
			break;
		}
		process_request(channel, buffer);
	}
	delete[] buffer;
	delete channel;
	if(channel == control_channel)
		channel = nullptr;
}

void send_signal_msg(SIGNAL_TYPE stype, string msg) {
	if(msg.empty() || msg == "" || control_channel == nullptr)
		return;

	sigmsg sig_msg(stype, (char*)msg.c_str(), msg.size());
	control_channel->cwrite(&sig_msg, sizeof(sigmsg));
}

int main (int argc, char* argv[]) {
	buffercapacity = MAX_MESSAGE;
	int opt;
	while ((opt = getopt(argc, argv, "m:")) != -1) {
		switch (opt) {
			case 'm':
				buffercapacity = atoi(optarg);
				break;
		}
	}

	srand(time_t(NULL));
	for (int i = 0; i < NUM_PERSONS; i++) {
		populate_file_data(i+1);
	}
	
	control_channel = new FIFORequestChannel("control", FIFORequestChannel::SERVER_SIDE);
	handle_process_loop(control_channel);
}
