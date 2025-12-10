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
vector<char> specialChars = {'\"','\\','$','`'};
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

vector<string> tokenizePATH(const string& path) {
  vector<string> pathTokens;
  stringstream p(path);
  string partpath;
  while(getline(p,partpath,pathDelimiter)) {
    pathTokens.emplace_back(partpath);
  }

  return pathTokens;
}

fs::path createPathTo(const string& filePath) {
  fs::path fPath , parentPath;
  if(filePath[0] == '/') {
    fPath = fs::path(filePath);
  }
  else {
    vector<string> temp = tokenizePATH(filePath);
    fPath = generatePath(temp);
  }

  parentPath = fPath.parent_path();
  if(!fs::exists(parentPath)) {
    fs::create_directories(parentPath);
  }

  return fPath;
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(const string& str : builtins) commands[str] = "sh";
  PATH = getenv("PATH");
  HOME = getenv("HOME");

  while(true) {
    cout << "$ ";
    bool append = false , overWrite = false;
    
    string cmd , str = "" , errorstr = "";
    getline(cin,cmd);

    vector<string> tokens = tokenize(cmd);
    if(tokens.empty()) continue;
    
    string outputFilePath = "";
    for(int i=0 ;i<tokens.size() ;i++) {
      if(tokens[i] == ">" || tokens[i] == "1>") {
        overWrite = true;
        outputFilePath = tokens[i+1];
        break;
      }
      else if(tokens[i] == ">>") {
        append = true;
        outputFilePath = tokens[i+1];
        break;
      }
    }

    if(cmd == "exit") {
      break;
    }
    else if(tokens[0] == "ls") {
      string sep = "\n";
      string p = "";

      if(tokens.size() > 1) {
        for(int i=1 ;i<tokens.size() ;i++) {
          if(tokens[i] == ">" || tokens[i] == "1>" || tokens[i] == ">>") break;
          if(tokens[i][0] == '-') {
            sep = " ";
          }
          else {
            p = tokens[i];
          }
        }
      }

      if(!p.size()) p = ".";

      for(const auto& files : fs::directory_iterator(p)) {
        str = str + files.path().filename().string()+sep;
      } 
    }
    else if(tokens[0] == "echo") {
      for(int i=1 ;i<tokens.size() ;i++) {
        if(tokens[i] == ">" || tokens[i] == "1>" || tokens[i] == ">>") {
          break;
        }

        str+=tokens[i];
        if(i != tokens.size()-1) str+=" ";
      }
      str+='\n';
    }
    else if(tokens[0] == "cat") {
      for(int i=1 ;i<tokens.size() ;i++) {
        if(!fs::exists(fs::path(tokens[i]))) {
          errorstr = "cat: " + tokens[i] + ": No such file or cannot open" + '\n';
          continue;
        }

        ifstream File(tokens[i]);
        string line;
        if (!File.is_open()) {
          errorstr = "cat: " + tokens[i] + ": File cannot be opened" + '\n';
          continue;
        }

        while(getline(File,line)) {
          str+=line;
        }
        File.close();
      }
      str+='\n';
    }
    else if(tokens[0] == "type") {
      for(int i=1 ; i<tokens.size() ;i++) {
        if(commands[tokens[i]] == "sh") {
          str = tokens[i] + " is a shell builtin";
        }
        else {
          fs::path p = checkExec(tokens[i]);
          if(!p.empty()) {
            str = tokens[i] + " is " + p.string();
          }
          else {
            errorstr = tokens[i] + ": not found" + '\n';
          } 
        }
        str+='\n';
      }
    }
    else if(tokens[0] == "pwd") {
      fs::path curr = fs::current_path();
      str = curr.string()+'\n';
    }
    else if(tokens[0] == "cd") {
      if(tokens.size() == 1) {
        fs::current_path(fs::path(HOME));
        continue;
      }

      vector<string> pathTokens = tokenizePATH(tokens[1]);

      if(tokens[1][0] == '/') {
        fs::path absPath = fs::path(tokens[1]);
        if(fs::exists(absPath) && fs::is_directory(absPath)) {
          fs::current_path(absPath);
        }
        else {
          errorstr = "cd: " + tokens[1] + ": No such file or directory" + '\n';
        }
      }
      else if(tokens[1][0] == '~') {
        fs::path curr = generatePath(pathTokens);

        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          errorstr = "cd: " + curr.string() + ": No such file or directory" + '\n';
        }
      }
      else {
        fs::path curr = generatePath(pathTokens);
        
        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          errorstr = "cd: " + curr.string() + ": No such file or directory" + '\n';
        }
      }
      
    }
    else {
      fs::path isExec = checkExec(tokens[0]);

      if(isExec.empty()) {
        errorstr = cmd + ": command not found" + '\n';
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

    if(!append && !overWrite) {
      if(str.size()) {
        cout<<str;
      }
      else {
        cerr<<errorstr;
      }
    }
    else {
      fs::path outputFile = createPathTo(outputFilePath);

      if(overWrite) {
        ofstream File(outputFile.string());

        if(File.is_open()) {
          File<<str;
          File.close();
        }
      }
      else {
        ofstream File(outputFile.string() , ios::app);
        if(File.is_open()) {
          File<<str;
          File.close();
        }
      } 
    }
  }

  return 0;
}