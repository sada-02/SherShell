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

    if(cmd == "exit") break;
    
    cout<<cmd<<": command not found"<<endl; 
  
  }

  return 0;
}
