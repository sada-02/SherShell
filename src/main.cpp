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
  #include <conio.h>   
  #include <windows.h>
  const char delimiter = ';';
  const char pathDelimiter = '/';

#else
  const char delimiter = ':';
  const char pathDelimiter = '/';
  #include <termios.h>
  #include <unistd.h>
#endif

map<string,string> commands;
vector<string> builtins = {"echo" , "exit" , "type" , "pwd" , "cd"};
vector<string> defaultcmds = {"echo" , "exit" , "type" , "pwd" , "cd" , "ls"};
vector<char> specialChars = {'\"','\\','$','`'};
string PATH;
string HOME;

#ifdef _WIN32
  DWORD orig_mode;
  HANDLE hStdin;

  void disableRawMode() {
    SetConsoleMode(hStdin, orig_mode);
  }

  void enableRawMode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &orig_mode);
    
    DWORD mode = orig_mode;
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    mode |= ENABLE_PROCESSED_INPUT;
    
    SetConsoleMode(hStdin, mode);
    atexit(disableRawMode);
  }

  char getChar() {
    return _getch();
  }

#else
  struct termios orig_termios;

  void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  }

  void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  }

  char getChar() {
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
  }
#endif

struct TrieNode {
  map<char,TrieNode*> ptrs;
  bool flag;

  TrieNode() {
    flag = false;
  }

};

class Trie {
  private :
  TrieNode* root ;

  public :
  Trie() {
    root = new TrieNode();
  }

  void insert(const string& str) {
    TrieNode* temp = root;

    for(int i=0 ;i<str.size() ;i++) {
      if(temp->ptrs.find(str[i]) == temp->ptrs.end()) {
        temp->ptrs[str[i]] = new TrieNode();
      }

      temp = temp->ptrs[str[i]];
    }

    temp->flag = true;
  }

  bool search(const string& str) {
    TrieNode* temp = root;

    for(int i=0 ;i<str.size() ;i++) {
      if(temp->ptrs.find(str[i]) == temp->ptrs.end()) {
        return false;
      }

      temp = temp->ptrs[str[i]];
    }

    return temp->flag;
  }

  vector<string> startWith(const string& str) {
    if(!str.size()) return vector<string> {};

    TrieNode* temp = root;
    for(int i=0 ;i<str.size() ;i++) {
      if(temp->ptrs.find(str[i]) == temp->ptrs.end()) {
        return vector<string> {};
      }

      temp = temp->ptrs[str[i]];
    }

    vector<string> found = {};
    for(auto const& part : temp->ptrs) {
      vector<string> t = findALL(part.second,str+part.first) ;
      found.insert(found.end() , t.begin() , t.end());
      t.clear();
    }

    if(found.empty()) return vector<string> {};
    else return found;
  }

  vector<string> findALL(TrieNode* temp , string str) {
    vector<string> found ;
    if(temp->flag) found.push_back(str);

    for(auto const& part : temp->ptrs) {
      vector<string> t = findALL(part.second,str+part.first) ;
      found.insert(found.end() , t.begin() , t.end());
      t.clear();
    }

    return found;
  }
};

Trie* checkAutoCompletion = new Trie();

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

vector<string> findExecWith(const string& str) {
  Trie* executablePaths = new Trie();
  string dir;
  stringstream path(PATH);

  while(getline(path , dir , delimiter)) {
    if(!fs::exists(fs::path(dir)) || !fs::is_directory(fs::path(dir))) continue;

    try {
      for(const auto& d : fs::directory_iterator(dir)) {
        if(fs::is_directory(d.path())) continue;
        
        auto perms = fs::status(d.path()).permissions();
        if((perms & fs::perms::owner_exec) != fs::perms::none ||
          (perms & fs::perms::group_exec) != fs::perms::none ||
          (perms & fs::perms::others_exec) != fs::perms::none) {
          executablePaths->insert(d.path().filename().string());    
        }
      }
    } 
    catch(...) {
      continue;
    }
  }

  vector<string> ret = executablePaths->startWith(str);
  delete executablePaths;
  return ret;
}

string readCommand() {
  string cmd = "" , temp = "";
  char c ;
  bool onetab = false;

  while(true) {
    c = getChar();

    if(c == '\n' || c == '\r') {
      cmd = cmd + temp;
      temp = "";
      cout<<'\n'<<flush;
      break;
    }
    else if(c == ' ') {
      cmd += temp + " ";
      temp = "";
      cout<<' '<<flush;
      onetab = false;
    }
    else if(c == '\t') {
      vector<string> words = checkAutoCompletion->startWith(temp);
      sort(words.begin() , words.end());

      if(words.size() == 1) {
        for(int i=0 ;i<temp.size() ;i++) cout<<"\b \b";
        cmd += *words.begin() + " ";
        cout<<*words.begin()<<' '<<flush;
        temp = "";
        onetab = false;
        continue;
      }

      words = findExecWith(temp);
      sort(words.begin() , words.end());
      if(words.size() == 1) {
        for(int i=0 ;i<temp.size() ;i++) cout<<"\b \b";
        cmd += *words.begin() + " ";
        cout<<*words.begin()<<' '<<flush;
        temp = "";
        onetab = false;
        continue;
      }

      if(onetab) {
        cout<<'\n';
        for(const string& s : words) {
          cout<<s<<"  ";
        }
        cout<<'\n'<<flush;
        cout<<"$ "<<cmd+temp<<flush;
        onetab = false;
      }
      else {
        cout<<'\x07'<<flush;
        onetab = true;
      }
    }
    else {
      temp += c;
      cout<<c<<flush;
      onetab = false;
    }
  }

  if(temp.size()) cmd+=temp;

  return cmd;
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

  if(!fs::exists(fPath)) {
    ofstream File(fPath.string());
    File.close();
  }


  return fPath;
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(const string& str : builtins) commands[str] = "sh";
  for(const string& str : defaultcmds) checkAutoCompletion->insert(str);
  PATH = getenv("PATH");
  HOME = getenv("HOME");

  enableRawMode();

  while(true) {
    cout << "$ ";
    bool append = false , overWrite = false , directop = false, directerr = false;
    string cmd , str = "" , errorstr = "";
    cmd = readCommand();

    vector<string> tokens = tokenize(cmd);
    if(tokens.empty()) continue;
    int maxIDX = tokens.size();
    
    string outputFilePath = "";
    for(int i=0 ;i<tokens.size() ;i++) {
      if(tokens[i] == ">" || tokens[i] == "1>" || tokens[i] == "2>") {
        overWrite = true;
        outputFilePath = tokens[i+1];
        maxIDX = i;
        directop = true;
        if(tokens[i] == "2>") {
          directerr = true;
        }
        break;
      }
      else if(tokens[i] == ">>" || tokens[i] == "1>>" || tokens[i] == "2>>") {
        append = true;
        outputFilePath = tokens[i+1];
        maxIDX = i;
        directop = true;
        if(tokens[i] == "2>>") {
          directerr = true;
        }
        break;
      }
    }

    if(cmd == "exit") {
      break;
    }
    else if(tokens[0] == "ls") {
      string sep = " ";
      string p = "";

      if(tokens.size() > 1) {
        for(int i=1 ;i<maxIDX ;i++) {
          if(tokens[i][0] == '-') {
            sep = "\n";
          }
          else {
            p = tokens[i];
          }
        }
      }

      if(!p.size()) p = ".";
      
      if(!fs::exists(fs::path(p))) {
        errorstr += "ls: " + p + ": No such file or directory\n";
      }
      else {
        vector<string> filesListed;
        for(const auto& files : fs::directory_iterator(p)) {
          filesListed.emplace_back(files.path().filename().string());
        } 

        sort(filesListed.begin() , filesListed.end());
        for(const string& s : filesListed) {
          str = str + s + sep;
        }
      }
    }
    else if(tokens[0] == "echo") {
      for(int i=1 ;i<maxIDX ;i++) {
        str+=tokens[i];
        if(i != tokens.size()-1) str+=" ";
      }
      str+='\n';
    }
    else if(tokens[0] == "cat") {
      for(int i=1 ;i<maxIDX ;i++) {
        if(!fs::exists(fs::path(tokens[i]))) {
          errorstr += "cat: " + tokens[i] + ": No such file or directory" + '\n';
          continue;
        }

        ifstream File(tokens[i]);
        string line;
        if (!File.is_open()) {
          errorstr += "cat: " + tokens[i] + ": File cannot be opened" + '\n';
          continue;
        }

        while(getline(File,line)) {
          str+=line;
          if(File.peek() != EOF) {
            str+='\n';
          }
        }
        File.close();
      }
      
      if(str.size()) str+='\n';
    }
    else if(tokens[0] == "type") {
      for(int i=1 ; i<maxIDX ;i++) {
        if(commands[tokens[i]] == "sh") {
          str = tokens[i] + " is a shell builtin" + '\n';
        }
        else {
          fs::path p = checkExec(tokens[i]);
          if(!p.empty()) {
            str = tokens[i] + " is " + p.string() + '\n';
          }
          else {
            errorstr += tokens[i] + ": not found" + '\n';
          } 
        }
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
          errorstr += "cd: " + tokens[1] + ": No such file or directory" + '\n';
        }
      }
      else if(tokens[1][0] == '~') {
        fs::path curr = generatePath(pathTokens);

        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          errorstr += "cd: " + curr.string() + ": No such file or directory" + '\n';
        }
      }
      else {
        fs::path curr = generatePath(pathTokens);
        
        if(fs::exists(curr) && fs::is_directory(curr)) {
          fs::current_path(curr);
        }
        else {
          errorstr += "cd: " + curr.string() + ": No such file or directory" + '\n';
        }
      }
      
    }
    else {
      fs::path isExec = checkExec(tokens[0]);

      if(isExec.empty()) {
        errorstr += cmd + ": command not found" + '\n';
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
        cout<<errorstr;
      }
    }
    else {
      fs::path outputFile = createPathTo(outputFilePath);

      if(directerr) {
        if(str.size()) {
          cout<<str;
        }

        if(errorstr.size()) {
          if(overWrite) {
            ofstream File(outputFile.string());
            if(File.is_open()) {
              File<<errorstr;
              File.close();
            }
          }
          else {
            ofstream File(outputFile.string() , ios::app);
            if(File.is_open()) {
              File<<errorstr;
              File.close();
            }
          }
        }
      }
      else {
        if(errorstr.size()) {
          cerr<<errorstr;
        }

        if(str.size()) {
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
    }
  }

  return 0;
}