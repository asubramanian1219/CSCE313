#include <iostream>
#include <vector>
#include <unistd.h>
#include <string>
#include <sys/wait.h>
#include <algorithm>
#include <fcntl.h>
#include <cstring>
#include <readline/readline.h>
#include <readline/history.h>


using namespace std;

string trim(string str){ //removes leading and trailing whitespace from string
    string spaces="\t\n\v\f\r ";
    str.erase(0,str.find_first_not_of(spaces));
    str.erase(str.find_last_not_of(spaces)+1);
    return str;
}

vector<string> parseCommands (string commands, char sep){ //separate a given string at all instances of a character, unless the separations are inside quotes
    vector<string> commandList; //list of commands

    vector<bool> quotes(commands.size()); //tells whether each character lies within quotes or not
    bool quote=false;
    for(int i=0;i<commands.size();i++){
        if(commands[i]=='\''|commands[i]=='\"'){
            quote=!quote;
        }
        quotes[i]=quote;
    }
    for(int i=0;i<commands.size();i++){ //cushions '<' and '>' with spaces if they are not there already.
        if((commands[i]=='<'|commands[i]=='>')&&(quotes[i]==false)){
            if(commands[i-1]!=' '&&commands[i+1]!=' '){
                auto it=quotes.begin()+i;
                commands.insert(i," ");
                quotes.insert(it,false);
                it=it+2;
                commands.insert(i+2," ");
                quotes.insert(it,false);
            }

        }
    }
    int index;
    string res;
    while(commands.size()) { //actually splits the strings up and puts them into the vector
        index = 10000;

        for (int i = 0; i < commands.size(); i++) {
            if (commands[i] == sep && quotes[i] == false) {
                index = i;
                break;
            }

        }
        res = commands.substr(0, index);
        res = trim(res);
        commands = commands.erase(0, index + 1);
        commandList.push_back(res);
        quotes.erase(quotes.begin(),quotes.begin()+index+1);

    }
    return commandList;

}

void execute(string command,vector<pid_t> pidlist){
    vector<string> comm=parseCommands(command,' '); //separates program name and arguments
    int fdwrite, fdread; //I/O files
    char temp1[FILENAME_MAX]; //used for storing the previous directory.
    char temp2[FILENAME_MAX];

    for(int i=0;i<comm.size();i++){ //removes quotes from arguments and trims them
        comm.at(i).erase(remove(comm.at(i).begin(),comm.at(i).end(),'\''),comm.at(i).end());
        comm.at(i).erase(remove(comm.at(i).begin(),comm.at(i).end(),'\"'),comm.at(i).end());
        comm.at(i)=trim(comm.at(i));
    }

    if(comm[0]=="jobs"){ //print all occurring processes
        for(int i=0;i<pidlist.size();i++){
            cout<<pidlist.at(i)<<endl;
        }
        return;
    }

    for(int i=0;i<comm.size();i++){ //redirects the stdout to the specified file
        if(comm[i]==">"){
            fdwrite=open(comm[i+1].c_str(), O_CREAT |O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(fdwrite,1);

            comm.erase(comm.begin()+i+1); //erases these "arguments" (the program won't actually use them
            comm.erase(comm.begin()+i);
        }
        else if(comm[i]=="<"){ //same thing, except with input
            fdread=open(comm[i+1].c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(fdread,0);

            comm.erase(comm.begin()+i);
        }
    }
    if(comm[0]=="cd"){
        if(comm[1]=="-"){
            getcwd(temp2,sizeof(temp2)); //stores the current directory in temp2
            //chdir(getenv("HOME"));
            chdir(temp1); //changes the directory to the previous directory
            strncpy(temp1,temp2,sizeof(temp2)); //makes the starting directory the new previous directory
        }
        else{
            getcwd(temp1, sizeof(temp1)); //changes the directory, that's all
            chdir(comm[1].c_str());
        }
        return; //end the program, nothing else needs to be done
    }


    char** args= new char * [comm.size()+1]; //change the command vector into a char* array to pass into execvp
    for(int i=0;i<comm.size();i++){
        args[i]=new char [comm[i].size()+1];
        strcpy(args[i], comm[i].c_str());
    }
    args[comm.size()]=NULL;

    execvp(args[0],args); //execute the program
}

int main() {

    int stdin=dup(0); //used to reset the stdin after a command is over
    vector<pid_t> pidlist; //list of PIDs
    vector<bool> exited;
    while(1){





        for(int i=0;i<pidlist.size();i++){
            //cout<<pidlist.at(i)<<endl;
            pid_t pr=waitpid(pidlist.at(i),0,WNOHANG);
            //cout<<pr<<endl;
            if(pr>0){
                exited.at(i)=true;
                cout<<"PID "<<pidlist.at(i)<<" exited"<<endl;
            } //wait for background process.

        }
        while(find(exited.begin(),exited.end(),true)!=exited.end()){
            for(int i=0;i<exited.size();i++){
                if(exited.at(i)==true){

                    exited.erase(exited.begin()+i);
                    pidlist.erase(pidlist.begin()+i);
                    break;
                }
            }
        }


        char temp[FILENAME_MAX];

        string prompt=getcwd(temp,sizeof(temp)); //get current directory and print it as part of the prompt
        prompt=prompt+"$ ";
        char promptArray[prompt.size()+1];
        strcpy(promptArray,prompt.c_str());
        char* commands=readline(promptArray); //prompt user, with autocomplete, of course
        if(strlen(commands)>0){ //if line was not empty, add it to the command history
            add_history(commands);
        }



        if(commands !=""){ //if the line isn't empty
            vector<string> splitCommand=parseCommands(commands,'|');
            size_t back;
            bool isBackground;
            for(int i=0;i<splitCommand.size();i++){ //for each pipe
                back=splitCommand.at(i).find('&'); //check if is a background process (designated by &)
                isBackground=false;
                if(back!=string::npos) {
                    isBackground = true;
                    splitCommand.at(i).erase(back);
                }
                int fd[2]; //open pipe
                pipe(fd);


                pid_t child=fork();


                if(child==0){ //if child process
                    if(i<splitCommand.size()-1){ //redirect output to next command's input, unless it's the last command
                        dup2(fd[1],1);
                        close(fd[1]);
                    }
                    cout<<getpid();
                    execute(splitCommand.at(i),pidlist); //execute the command






                }
                else{
                    if(isBackground==1){
                        pidlist.push_back(child); //add process to the pidlist
                        exited.push_back(false);
                        cout<<"Background process"<<endl;
                        dup2(fd[0],0); //change next process's stdin to that of the pipe's
                        close(fd[1]);
                    }
                    else{
                        waitpid(child,0,0); //blockingly wait
                        dup2(fd[0],0); //same as before
                        close(fd[1]);
                    }
                }

            }
            dup2(stdin, 0);

        }

    }

}
