/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/20
 */
#include "common.h"
#include "FIFOreqchannel.h"
#include <getopt.h>
#include <sys/wait.h>

using namespace std;


int main(int argc, char *argv[]){
    int n = 100;    // default number of requests per "patient"
    int p = 15;		// number of patients

    bool pflag=0;  //The flags from command line.
    bool tflag=0;
    bool eflag=0;
    bool fflag=0;
    bool cflag=0;

    srand(time_t(NULL));

    int patient_num; //options
    double ecg_time;
    int ecg_type;

    int opt;
    int def; //default case, dummy variable
    string filename="";
    int status;
    int buffer_size;

    pid_t child_pid=fork(); //start server
    if (child_pid==0){
        cout<<"Starting server"<<endl;
        execvp("./server",argv);
        perror("exec failed");
    }
    else if (child_pid>0){ //if this is the parent process
        while((opt=getopt(argc,argv,"p:t:e:f:m:c"))!=-1){ //get command line arguments
            switch(opt){
                case'p':
                    pflag=1;
                    patient_num=atoi(optarg);
                    break;
                case 't':
                    tflag=1;
                    ecg_time=atof(optarg);
                    break;
                case 'e':
                    eflag=1;
                    ecg_type=atoi(optarg);
                    break;
                case 'f':
                    fflag=1;
                    filename=optarg;
                    break;
                case 'm':
                    buffer_size=atoi(optarg);
                    break;
                case 'c':
                    cflag=1;
                    break;
                default:
                    def=-1;
                    break;
            }

        }
        if((pflag)&&(tflag)&&(eflag)){ //request single point
            FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE); //open channel
            datamsg data=datamsg(patient_num,ecg_time,ecg_type); //request point
            chan.cwrite(&data,sizeof(data)); //write message to channel
            char* buf=(char*)malloc(sizeof(double)); //buffer to take the server's output
            int numBytes=chan.cread(buf,sizeof(double)); //get server's output
            double ecg_val=*(double*)buf;
            cout<<"ECG VAL "<<ecg_type<<" is "<<ecg_val<<endl;
            MESSAGE_TYPE m=QUIT_MSG; //quit the client
            chan.cwrite(&m,sizeof(MESSAGE_TYPE));
            delete buf;


        }
        else if((pflag)&&(!tflag)&&(!eflag)){ //request all 15k lines
            timeval start;
            timeval end;
            gettimeofday(&start,NULL);
            FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
            cout<<"Started data points"<<endl;
            string outputFileName="received/x"+to_string(patient_num)+".csv";
            cout<<outputFileName<<endl;

            ofstream outputFile("received");
            outputFile.open(outputFileName, ios::out | ios::binary);
            for(double i=0;i<59.996;i=i+0.004){
                datamsg data1=datamsg(patient_num,i,1);
                chan.cwrite(&data1,sizeof(data1));
                char* buf1=(char*)malloc(sizeof(double));
                int numBytes1=chan.cread(buf1,sizeof(double));
                double ecg_val1=*(double*)buf1;

                datamsg data2=datamsg(patient_num,i,2);
                chan.cwrite(&data2,sizeof(data2));
                char* buf2=(char*)malloc(sizeof(double));
                int numBytes2=chan.cread(buf2,sizeof(double));
                double ecg_val2=*(double*)buf2;

                outputFile<<i<<","<<ecg_val1<<","<<ecg_val2<<endl;
                delete buf1;
                delete buf2;
            }
            outputFile.close();
            gettimeofday(&end,NULL);
            double time_taken;
            time_taken=(end.tv_sec-start.tv_sec)*1e6; //seconds
            time_taken=(time_taken+(end.tv_usec-start.tv_usec))*1e-6; //microseconds
            cout<<"Time taken: "<<fixed<<time_taken<<setprecision(6)<<" sec"<<endl;

            MESSAGE_TYPE m =QUIT_MSG;
            chan.cwrite(&m, sizeof(MESSAGE_TYPE));


        }
        else if(fflag){
            timeval start;
            timeval end;


            gettimeofday(&start,NULL);
            FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
            ofstream outputFile("received");
            string outputFileName="received/"+filename;
            outputFile.open(outputFileName, ios::out | ios::binary);
            cout<<"Requesting "<<filename<<endl;
            filemsg f=filemsg(0,0); //get file's size

            char* buf=new char[sizeof(filemsg)+filename.length()+1]; //write header and the null terminated file name to the channel
            memcpy(buf,(char*)&f,sizeof(filemsg));
            strcpy(buf+sizeof(filemsg),filename.c_str());

            int req=chan.cwrite(buf,sizeof(filemsg)+filename.length()+1); //ask for the file size.
            char* temp=(char*)malloc(sizeof(__int64_t));
            int fs=chan.cread(temp,sizeof(__int64_t)); //get it
            __int64_t fileSize=*(__int64_t*)temp;
            cout<<"File size is "<<fileSize<<endl;
            delete temp;
            delete buf;

            int lim=fileSize/buffer_size; //number of requests needed
            int count=0;
            int offset=0;


            cout<<"Transferring file in "<<lim<<" requests."<<endl; //getting the file and writing to output
            while(count<lim){
                filemsg req1=filemsg(offset,buffer_size);
                char* buf2=new char[sizeof(filemsg)+filename.length()+1];
                memcpy(buf2, (char*)&req1,sizeof(filemsg));
                strcpy(buf2+sizeof(filemsg),filename.c_str());
                int request2=chan.cwrite(buf2,sizeof(filemsg)+filename.length()+1);
                char* toWrite=(char*)malloc(buffer_size);
                int e=chan.cread(toWrite,buffer_size);
                count++;
                offset+=buffer_size;
                outputFile.write(toWrite,buffer_size);
                delete buf2;
                delete toWrite;
            }
            if(fileSize-offset>0){ //if there are any leftovers write them too.
                filemsg req1=filemsg(offset,fileSize-offset);
                char* buf2=new char[sizeof(filemsg)+filename.length()+1];
                memcpy(buf2, (char*)&req1,sizeof(filemsg));
                strcpy(buf2+sizeof(filemsg),filename.c_str());
                int request2=chan.cwrite(buf2,sizeof(filemsg)+filename.length()+1);
                char* toWrite=(char*)malloc(fileSize-offset);
                int e=chan.cread(toWrite,fileSize-offset);
                outputFile.write(toWrite,fileSize-offset);
                delete buf2;
                delete toWrite;
            }
            //get time taken and quit.
            gettimeofday(&end,NULL);
            double time_taken;

            time_taken=(end.tv_sec-start.tv_sec)*1e6;
            time_taken=(time_taken+(end.tv_usec-start.tv_usec))*1e-6;
            cout<<"Time taken: "<<fixed<<time_taken<<setprecision(6)<<" sec"<<endl;

            MESSAGE_TYPE m =QUIT_MSG;
            chan.cwrite(&m, sizeof(MESSAGE_TYPE));



        }
        else if(cflag){
            cout<<"Opening new channel: "<<endl;
            FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);

            MESSAGE_TYPE newchannel=NEWCHANNEL_MSG; //ask for new channel
            chan.cwrite(&newchannel,sizeof(MESSAGE_TYPE)); //write to pipe
            char* temp=(char*)malloc(sizeof(string)); //pointer for channel name
            int waste=chan.cread(temp,sizeof(string)); //get channel name
            string chanName=temp;

            cout<<"Channel name is "<<chanName<<endl; //print channel name
            FIFORequestChannel chan2 (chanName,FIFORequestChannel::CLIENT_SIDE); //open new channel
            datamsg data1=datamsg(10,59,1); //request point through new channel.
            chan2.cwrite(&data1,sizeof(datamsg));

            char* buf=(char*)malloc(sizeof(double));
            int numBytes=chan2.cread(buf,sizeof(double));
            double ecg_val=*(double*)buf;
            cout<<"ECG VAL "<<1<<" is "<<ecg_val<<endl; //quit both channels.
            MESSAGE_TYPE m=QUIT_MSG;
            chan2.cwrite(&m, sizeof(MESSAGE_TYPE));
            chan.cwrite(&m,sizeof(MESSAGE_TYPE));

        }
        else{
            cout<<"ERROR"<<endl;
        } //wait for child to finish.
        pid_t pid=wait(&status);
        if(WIFEXITED(status)){
            printf("Parent: Child exited with status: %d\n", WEXITSTATUS(status));
        }

    }
    else{
        cout<<"Something's wrong."<<endl;
    }


}
