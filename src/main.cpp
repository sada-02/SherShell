#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <filesystem>
#include <sys/wait.h>
using namespace std;
namespace fs = filesystem;

#ifdef _WIN32
  const char delimiter = ';';
#else
  const char delimiter = ':';
#endif

map<string,string> commands;
vector<string> builtins = {"echo" , "exit" , "type"};
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

fs::path checkExec(const string& leftOver) {
  string dir;
  stringstream path(PATH);
  bool flag = false;

  while(getline(path , dir , delimiter)) {
    fs::path fullPath = fs::path(dir) / leftOver;

    if(fs::exists(fullPath) && !fs::is_directory(fullPath)) {
      auto perms = fs::status(fullPath).permissions();
      if((perms & fs::perms::owner_exec) != fs::perms::none ||
        (perms & fs::perms::group_exec) != fs::perms::none ||
        (perms & fs::perms::others_exec) != fs::perms::none) {
        return fullPath;
      }
    }
  }

  return {};
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(const string& str : builtins) commands[str] = "sh";
  PATH = getenv("PATH");

  while(true) {
    cout << "$ ";
    
    string cmd;
    getline(cin,cmd);

    vector<string> tokens = tokenize(cmd);
    if(tokens.empty()) continue;

    if(cmd == "exit") {
      break;
    }
    else if(tokens[0] == "echo") {
      cout<<cmd.substr(5)<<endl;
    }
    else if(tokens[0] == "type") {
      string leftOver = cmd.substr(5);

      if(commands[leftOver] == "sh") {
        cout<<leftOver<<" is a shell builtin"<<endl;
      }
      else {
        fs::path p = checkExec(leftOver);
        if(!p.empty()) {
          cout<<leftOver<<" is "<<p.string()<<endl;
        }
        else {
          cout<<leftOver<<": not found"<<endl;
        } 
      }
    }
    else {
      fs::path isExec = checkExec(tokens[0]);

      if(isExec.empty()) {
        cout<<cmd<<": command not found"<<endl;
      }
      else {
        vector<char*> args;
        for(auto& t : tokens) {
          args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(NULL);

        pid_t pid = fork();

        if(pid == 0) {
          execvp(isExec , args.data());
        }
        else {
          int status;
          waitpid(pid,&status,0);
        }
      }
    }
  }

  return 0;
}