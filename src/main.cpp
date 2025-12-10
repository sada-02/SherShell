#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <filesystem>
#include <sys/wait.h>
#include <fstream>
using namespace std;
namespace fs = filesystem;

#ifdef _WIN32
  const char delimiter = ';';
  const char pathDelimiter = '/';

#else
  const char delimiter = ':';
  const char pathDelimiter = '/';
#endif

map<string,string> commands;
vector<string> builtins = {"echo" , "exit" , "type" , "pwd" ,"cd"};
vector<char> specialChars = {'\"',"\\","$","`"}
string PATH;
string HOME;

vector<string> tokenize(string& query) {
  vector<string> tokens ;
  string temp = "";
  bool insinglequotes = false , indoublequotes = false , escapeON = false;

  for(int i=0 ;i<query.size() ;i++) {
    if(query[i] == ' ') {
      if(temp.size()) {
        if((!insinglequotes && !indoublequotes) && !escapeON) {
          tokens.emplace_back(temp);
          temp = "";
        }
        else {
          if(escapeON) {
            escapeON = !escapeON;
          }
          
          temp+=query[i];
        }
      }
    }
    else if(query[i] == '\\') {
      if(insinglequotes) {
        temp+='\\';
      }
      else {
        if(escapeON) {
          temp+='\\';
        }

        escapeON = !escapeON;
      }
    }
    else if(query[i] == '\'') {
      if(indoublequotes) {
        if(escapeON) {
          escapeON = !escapeON;
          temp+='\\';
        }
        temp+=query[i];
      }
      else if(escapeON) {
        escapeON = !escapeON;
        temp+=query[i];
      }
      else {
        insinglequotes = !insinglequotes;
      }
    }
    else if(query[i] == '\"') {
      if(insinglequotes) {
        temp+=query[i];
      }
      else if(escapeON) {
        escapeON = !escapeON;
        temp+=query[i];
      }
      else {
        indoublequotes = !indoublequotes;
      }
    }
    else {
      if(escapeON) {
        escapeON = !escapeON;
      }
      else {
        if(indoublequotes) {
          bool f = false;
          for(char c : specialChars) {
            if(c == query[i]) {
              f = true;
            }
          }

          if(!f) temp+='\\';
        }
      }
      temp+=query[i];
    }
  }

  if(temp.size()) {
    if(insinglequotes) {
      tokens.emplace_back("\'"+temp);
    }
    else if(indoublequotes) {
      tokens.emplace_back("\""+temp);
    }
    else {
      tokens.emplace_back(temp);
    }
    
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

fs::path generatePath(vector<string>& pathTokens) {
  fs::path curr;
  if(pathTokens[0] == "~") {
    curr = fs::path(HOME);
  }
  else {
    curr = fs::current_path();
  }

  for(int i=0 ;i<pathTokens.size() ;i++) {
    if(pathTokens[i] == ".") {
      continue;
    }
    else if(pathTokens[i] == "~") {
      curr = fs::path(HOME);
    }
    else if(pathTokens[i] == "..") {
      curr = curr.parent_path();
    }
    else {
      curr = curr/pathTokens[i];
    }
  }

  return curr;
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(const string& str : builtins) commands[str] = "sh";
  PATH = getenv("PATH");
  HOME = getenv("HOME");

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
      for(int i=1 ;i<tokens.size() ;i++) {
        cout<<tokens[i];
        if(i != tokens.size()-1) cout<<" ";
      }
      cout<<endl;
    }
    else if(tokens[0] == "cat") {
      for(int i=1 ;i<tokens.size() ;i++) {
        if(!fs::exists(fs::path(tokens[i]))) {
          cerr<<"cat: "<<tokens[i]<<": No such file or cannot open"<<endl;
          continue;
        }

        ifstream File(tokens[i]);
        string line;
        if (!File.is_open()) {
          cerr<<"cat: "<<tokens[i]<<": File cannot be opened"<<endl;
          continue;
        }

        while(getline(File,line)) {
          cout<<line;
        }
        File.close();
      }
      cout<<endl;
    }
    else if(tokens[0] == "type") {
      string leftOver = "";
      for(int i=1 ; i<tokens.size() ;i++) {
        leftOver += tokens[i] ;
        if(i != tokens.size()-1) leftOver+=" ";
      }

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
    else if(tokens[0] == "pwd") {
      fs::path curr = fs::current_path();
      cout<<curr.string()<<endl;
    }
    else if(tokens[0] == "cd") {
      if(tokens.size() == 1) {
        fs::current_path(fs::path(HOME));
        continue;
      }

      string path = tokens[1];
      vector<string> pathTokens;
      stringstream p(path);
      string partpath;
      while(getline(p,partpath,pathDelimiter)) {
        pathTokens.emplace_back(partpath);
      }

      if(path[0] == '/') {
        fs::path absPath = fs::path(path);
        if(fs::exists(absPath) && fs::is_directory(absPath)) {
          fs::current_path(absPath);
        }
        else {
          cout<<"cd: "<<path<<": No such file or directory"<<endl;
        }
      }
      else if(path[0] == '~') {
        fs::path curr = generatePath(pathTokens);

        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          cout<<"cd: "<<curr.string()<<": No such file or directory"<<endl;
        }
      }
      else {
        fs::path curr = generatePath(pathTokens);
        
        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          cout<<"cd: "<<curr.string()<<": No such file or directory"<<endl;
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
          execvp(isExec.string().c_str() , args.data());
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