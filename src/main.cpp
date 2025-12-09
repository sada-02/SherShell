#include <iostream>
#include <string>
using namespace std;

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  while(true) {
    cout << "$ ";
    string cmd;
    getline(cin,cmd);

    if(cmd == "exit") {
      break;
    }
    else if(cmd.substr(0,4) == "echo" && cmd[4] == ' ') {
      cout<<cmd.substr(5)<<endl;
    }
    else {
      cout<<cmd<<": command not found"<<endl; 
    }
  }

  return 0;
}
