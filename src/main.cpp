#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>
#include <sstream>
#include <unistd.h>
using namespace std;

map<string,string> commands;

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  commands["echo"] = "sh";
  commands["exit"] = "sh";
  commands["type"] = "sh";

  string PATH = getenv("PATH");

  while(true) {
    cout << "$ ";
    string cmd;
    getline(cin,cmd);

    if(cmd == "exit") {
      break;
    }
    else if(cmd.substr(0,5) == "echo ") {
      cout<<cmd.substr(5)<<endl;
    }
    else if(cmd.substr(0,5) == "type ") {
      string leftOver = cmd.substr(5);

      if(commands[leftOver] == "sh") {
        cout<<leftOver<<" is a shell builtin"<<endl;
      }
      else {
        string dir;
        stringstream path(PATH);
        bool flag = false;

        while(getline(path , dir , ':')) {
          string fullPath = dir + "/" + leftOver;

          struct stat sb;
          if(stat(fullPath.c_str() , &sb) == 0) {
            if(access(fullPath.c_str() , X_OK) == 0) {
              cout<<leftOver<<" is "<<fullPath<<endl;
              flag = true;
            }
          }

          if(flag) break;
        }
        if(!flag)
        
        cout<<leftOver<<": not found"<<endl;
      }
    }
    else {
      cout<<cmd<<": command not found"<<endl; 
    }
  }

  return 0;
}
