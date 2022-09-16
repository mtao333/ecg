#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <fstream>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include <vector>

#define NUM_PERSONS 15  // number of person to collect data for
#define MAX_MESSAGE 256 // maximum buffer size for each message

typedef char byte_t;


// different types of messages
enum MESSAGE_TYPE {UNKNOWN_MSG, DATA_MSG, FILE_MSG, NEWCHANNEL_MSG, QUIT_MSG, SIGNAL_MSG};

enum SIGNAL_TYPE {ERROR, INFO};

class sigmsg {
public:
    MESSAGE_TYPE mtype;
    SIGNAL_TYPE stype;
    char msg[MAX_MESSAGE];

    sigmsg(SIGNAL_TYPE _stype, char* _msg_ptr, int _msg_size) {
        mtype = SIGNAL_MSG;
        stype = _stype;
        memset(msg, 0, MAX_MESSAGE);
        memcpy(msg, _msg_ptr, _msg_size);
    }
};

// message requesting a data point
class datamsg {
public:
    MESSAGE_TYPE mtype;
    int person;
    double seconds;
    int ecgno;

    datamsg (int _person, double _seconds, int _eno) {
        mtype = DATA_MSG;
        person = _person;
        seconds = _seconds;
        ecgno = _eno;
    }
};


// message requesting a file
class filemsg {
public:
    MESSAGE_TYPE mtype;
    __int64_t offset;
    int length;
	    
    filemsg (__int64_t _offset, int _length) {
        mtype = FILE_MSG;
        offset = _offset;
        length = _length;
    }
};

void EXITONERROR (std::string msg);
std::vector<std::string> split (std::string line, char separator);
__int64_t get_file_size (std::string filename);

#endif
