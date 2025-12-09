#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <filesystem>
using namespace std;
namespace fs = filesystem;

map<string,string> commands;
vector<fs::path> execFiles;
vector<string> builtins = {"echo" , "exit" , "type" , "exit"};
string PATH;

vector<string> tokenize(string& query) {
  stringstream q(query);
  string temp;
  vector<string> tokens;

  while(getline(q,temp,' ')) {
    tokens.emplace_back(temp);
  }

  return tokens;
}

void getExecFiles() {
  int start=0;
  for(int i=0 ;i<PATH.size() ;i++) {
    if(PATH[i] == ':') {
      string p = PATH.substr(start , i-start);

      if(fs::exists(p)) {
        for(const auto& DIR : fs::directory_iterator(p)) {
          if(!DIR.is_directory() && (DIR.status().permissions() & fs::perms::owner_exec)==fs::perms::owner_exec) {
            execFiles.emplace_back(DIR.path());
          }
        }
      }

      start = i+1;
    }
  }
}

fs::path findPath(string& s) {
  for(const auto& p :execFiles) {
    if(p.stem() == s) {
      return p;
    }
  }

  return fs::path{};
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(string& b : builtins) commands[b] = "sh";
  PATH = getenv("PATH");
  PATH.append(":");
  getExecFiles();

  while(true) {
    cout << "$ ";
    string cmd;
    getline(cin,cmd);

    vector<string> tokens = tokenize(cmd);

    if(tokens[0] == "exit") {
      break;
    }
    else if(tokens[0] == "echo") {
      for(string& str : tokens) {
        cout<<str<<" "<<endl;
      }
    }
    else if(tokens[0] == "type") {
      string leftOver = cmd.substr(5);

      if(commands[leftOver] == "sh") {
        cout<<leftOver<<" is a shell builtin"<<endl;
      }
      else {
        auto p = findPath(leftOver);
        if(!p.empty()) {
          cout<<leftOver<<" is "<<p.string()<<endl;
        }
        else {
          cout<<leftOver<<": not found"<<endl;
        } 
      }
    }
    else {
      cout<<cmd<<": command not found"<<endl; 
    }
  }

  return 0;
}
