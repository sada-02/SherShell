#include <iostream>
#include <vector>
#include <string>
#include <map>
using namespace std;

map<string,string> commands;

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  commands["echo"] = "sh";
  commands["exit"] = "sh";
  commands["type"] = "sh";

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
        cout<<leftOver<<": not found"<<endl;
      }
    }
    else {
      cout<<cmd<<": command not found"<<endl; 
    }
  }

  return 0;
}
