#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "ReqChannel.h"
#include "SHMreqchannel.h"
#include <time.h>
#include <thread>
using namespace std;
/*
void timediff (struct timeval& start, struct timeval& end){

}
*/

RequestChannel* create_new_channel (RequestChannel* mainchan, string ival, int mb){
    char name [1024];
    MESSAGE_TYPE m=NEWCHANNEL_MSG;
    mainchan->cwrite(&m,sizeof(m));
    mainchan->cread (name,1024);
    RequestChannel* newchan=0;
    if (ival == "f")
        newchan=new FIFORequestChannel (name, RequestChannel::CLIENT_SIDE);
    else if (ival=="q")
        newchan=new MQRequestChannel (name, RequestChannel::CLIENT_SIDE, mb);
    else if (ival=="s")
        newchan=new SHMRequestChannel (name, RequestChannel::CLIENT_SIDE, mb);
    return newchan;

}

void patient_thread_function(int n, int pno, BoundedBuffer* request_buffer){
    /* What will the patient threads do? */
    datamsg d (pno, 0.0, 1);
    double resp=0;
    for (int i=0;i<n;i++){
        request_buffer->push ((char*)&d, sizeof(datamsg));
        d.seconds+=0.004;
    }
}

void file_thread_function(string fname, BoundedBuffer* request_buffer, RequestChannel* chan, int mb){
    //1. create file
    string recvfname="recv/"+fname;

    //make it as long as the original length
    char buf [1024];
    filemsg f (0,0);
    memcpy (buf, &f, sizeof(f));
    strcpy (buf+sizeof(f),fname.c_str());
    chan->cwrite(buf, sizeof(f)+fname.size()+1);
    __int64_t filelength;
    chan->cread(&filelength, sizeof(filelength));

    FILE* fp=fopen(recvfname.c_str(), "w");
    fseek(fp,filelength, SEEK_SET);
    fclose(fp);
    //2. generate file messages
    filemsg* fm=(filemsg *) buf;
    __int64_t remlen=filelength;

    while(remlen>0){
        fm->length = min(remlen, (__int64_t) mb);
        request_buffer->push(buf,sizeof(filemsg)+fname.size()+1);
        fm->offset+=fm->length;
        remlen-=fm->length;
    }
}

void worker_thread_function(RequestChannel* chan, BoundedBuffer* request_buffer, HistogramCollection* hc, int mb){
    /*
		Functionality of the worker threads	
    */
    char buf[1024];
    double resp=0;

    char recvbuf[mb];
    while(true){
        request_buffer->pop(buf,1024);
        MESSAGE_TYPE* m = (MESSAGE_TYPE *) buf;
        //double resp=0;

        if(*m==DATA_MSG){
            chan->cwrite(buf,sizeof(datamsg));
            chan->cread(&resp, sizeof(double));
            hc->update(((datamsg*)buf)->person,resp);
        }
        else if(*m==QUIT_MSG){
            chan->cwrite(m,sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
        else if(*m==FILE_MSG){
            filemsg* fm=(filemsg*) buf;
            string fname=(char*)(fm+1);
            int sz=sizeof(filemsg)+fname.size()+1;
            chan->cwrite(buf,sz);
            chan->cread(recvbuf,mb);

            string recvfname="recv/"+fname;

            FILE* fp=fopen(recvfname.c_str(), "r+");
            fseek (fp, fm->offset, SEEK_SET);
            fwrite(recvbuf, 1, fm->length, fp);
            fclose(fp);
        }
    }
}



int main(int argc, char *argv[])
{
    int n = 15000;    //default number of requests per "patient"
    int p = 1;     // number of patients [1,15]
    int w = 200;    //default number of worker threads
    int b = 500; 	// default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE; 	// default capacity of the message buffer
    srand(time_t(NULL));
    bool fflag=0;
    string fname="";
    string ival="f";

    int opt = -1;
    while((opt=getopt(argc, argv, "m:n:p:w:f:b:i:"))!=-1){
        switch(opt){
            case 'm':
                m=atoi(optarg);
                break;
            case 'n':
                n=atoi(optarg);
                break;
            case 'p':
                p=atoi(optarg);
                break;
            case 'w':
                w=atoi(optarg);
                break;
            case 'f':
                fflag=1;
                fname=optarg;
                break;
            case 'b':
                b=atoi(optarg);
                break;
            case 'i':
                ival=optarg;
                break;


        }
    }


    int pid = fork();
    if (pid == 0){
        // modify this to pass along m
        execl ("server", "server", "-m", to_string(m).c_str(), "-i", ival.c_str(), (char *)NULL);
    }

    RequestChannel* chan;
    if(ival=="f")
        chan= new FIFORequestChannel("control", RequestChannel::CLIENT_SIDE);
    else if (ival=="q")
        chan=new MQRequestChannel ("control", RequestChannel::CLIENT_SIDE, m);
    else if (ival=="s")
        chan=new SHMRequestChannel("control", RequestChannel::CLIENT_SIDE, m);
    BoundedBuffer request_buffer(b);
    HistogramCollection hc;

    //making histogram and adding to the histogram collection hc
    for (int i=0;i<p;i++){
        Histogram* h = new Histogram (10, -2.0, 2.0);
        hc.add(h);

    }

    //making the worker channels
    RequestChannel* wchans [w];
    for (int i=0;i<w;i++){
        wchans [i] = create_new_channel(chan, ival, m);
    }



    struct timeval start, end;
    gettimeofday (&start, 0);

    if(fflag==0) {

        /* Start all threads here */

        thread patient [p];
        for (int i=0;i<p;i++){
            patient [i] = thread(patient_thread_function, n, i+1, &request_buffer);
        }

        //thread filethread(file_thread_function, fname, &request_buffer, chan, m);

        thread workers[w];
        for (int i = 0; i < w; i++) {
            workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);
        }

        /* Join all threads here */

        for (int i=0;i<p;i++){
            patient [i].join();
        }

        //filethread.join();
        cout << "Patient threads/file threads finished" << endl;
        for (int i = 0; i < w; i++) {
            MESSAGE_TYPE q = QUIT_MSG;
            request_buffer.push((char *) &q, sizeof(q));
        }
        for (int i = 0; i < w; i++) {
            workers[i].join();
        }
        cout << "Worker threads finished" << endl;
    }
    else{
        /* Start all threads here */
        /*
        thread patient [p];
        for (int i=0;i<p;i++){
            patient [i] = thread(patient_thread_function, n, i+1, &request_buffer);
        }*/

        thread filethread(file_thread_function, fname, &request_buffer, chan, m);

        thread workers[w];
        for (int i = 0; i < w; i++) {
            workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);
        }

        /* Join all threads here */
        /*
        for (int i=0;i<p;i++){
            patient [i].join();
        }
        */
        filethread.join();
        cout << "Patient threads/file threads finished" << endl;
        for (int i = 0; i < w; i++) {
            MESSAGE_TYPE q = QUIT_MSG;
            request_buffer.push((char *) &q, sizeof(q));
        }
        for (int i = 0; i < w; i++) {
            workers[i].join();
        }
        cout << "Worker threads finished" << endl;
    }
    gettimeofday (&end, 0);
    // print the results
    hc.print ();


    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;

}