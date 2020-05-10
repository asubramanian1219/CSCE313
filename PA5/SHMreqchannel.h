#ifndef _SHMreqchannel_H_
#define _SHMreqchannel_H

#include "common.h"
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

class SMBB{
private:
    string name;
    sem_t* producerdone;
    sem_t* consumerdone;

    int shmfd;
    char* data;
    int buffersize;

public:
    SMBB(string _name, int _bufsz): buffersize(_bufsz), name(_name){
        shmfd = shm_open(name.c_str(), O_RDWR|O_CREAT, 0644);
        if(shmfd<0){
            perror("SHM Create Error");
            exit(0);
        }
        ftruncate(shmfd, buffersize);
        data=(char*) mmap(NULL, buffersize, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
        if(!data){
            perror("Map error");
            exit(0);
        }
        producerdone=sem_open((name+"1").c_str(),O_CREAT,0644,0);
        consumerdone=sem_open((name+"2").c_str(),O_CREAT,0644,0);

    }
    ~SMBB(){
        munmap(data, buffersize);
        close (shmfd);
        shm_unlink(name.c_str());

        sem_close (producerdone);
        sem_close (consumerdone);

        sem_unlink ((name+"1").c_str());
        sem_unlink ((name+"2").c_str());

    }

    int push (char* msg, int len){
        sem_wait(consumerdone);
        memcpy(data,msg,len);
        sem_post(producerdone);
        return len;
    }

    int pop (char* msg, int len){
        sem_wait (producerdone);
        memcpy (msg, data, len);
        sem_post (consumerdone);
        return len;
    }
};

class SHMRequestChannel: public RequestChannel{
private:
    SMBB* b1;
    SMBB* b2;
    string s1, s2;
    int buffersize;
public:
    SHMRequestChannel(const string _name, const Side _side, int _bufs):RequestChannel(_name, _side), buffersize(_bufs){
        s1 = "/bb_"+_name+"1";
        s2 = "/bb_"+_name+"2";

        if(_side==SERVER_SIDE){
            b1=new SMBB(s1,buffersize);
            b2=new SMBB(s2, buffersize);
        }
        else{
            b1=new SMBB (s2,buffersize);
            b2=new SMBB (s1, buffersize);
        }
    }
    ~SHMRequestChannel(){
        delete b1;
        delete b2;
    }
    int cread(void* buf, int len){
        return b1->pop ((char*) buf, len);
    }
    int cwrite(void* buf, int len){
        return b2->push ((char*) buf, len);
    }
};
#endif