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
vector<string> builtins = {"echo" , "exit" , "type" , "pwd" , "cd" , "history"};
vector<string> defaultcmds = {"echo" , "exit" , "type" , "pwd" , "cd" , "ls" , 
  "cat" , "history"};
vector<char> specialChars = {'\"','\\','$','`'};
string PATH;
string HOME;
string HISTORYFILE;
vector<string> HISTORY;
int currHistPtr ;
vector<char> extensions;
int lastAppend;
char* histfileEnv;
fs::path histFilePath;

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

bool is_number(const string& s) {
  string::const_iterator it = s.begin();
  while (it != s.end() && isdigit(*it)) ++it;
  return !s.empty() && it == s.end();
}

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

string longestCommonPrefix(vector<string>& words) {
  if(words.empty()) return "";

  string lcp = words[0];
  for(int i=1 ;i<words.size() ;i++) {
    for(int j=0 ;j<lcp.size() && j<words[i].size() ;j++) {
      if(words[i][j] != lcp[j]) {
        lcp = lcp.substr(0,j);
        break;
      }
    }
  }  

  return lcp;
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
    else if(c == '\x1b') {
      char seq1 = getChar();
      char seq2 = getChar();
      
      if(seq1 == '[') {
        if(seq2 == 'A') {
          if(currHistPtr > 0 && HISTORY.size() > 0) {
            for(int i=0; i<temp.size()+cmd.size(); i++) cout<<"\b \b";
            currHistPtr--;
            cmd = HISTORY[currHistPtr];
            cout<<cmd<<flush;
            temp = "";
          }
        }
        else if(seq2 == 'B') {
          if(currHistPtr < HISTORY.size()) {
            for(int i=0; i<temp.size()+cmd.size(); i++) cout<<"\b \b";
            currHistPtr++;
            if(currHistPtr >= HISTORY.size()) {
              cmd = "";
              temp = "";
            } 
            else {
              cmd = HISTORY[currHistPtr];
              cout<<cmd<<flush;
              temp = "";
            }
          }
        }
      }
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
      else if(words.size() > 1) {
        string lcp = longestCommonPrefix(words);
        if(lcp != temp) {
          for(int i=0 ;i<temp.size() ;i++) cout<<"\b \b";
          cout<<lcp<<flush;
          temp = lcp;
          onetab = false;
          continue;
        }
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
      else if(words.size() > 1) {
        string lcp = longestCommonPrefix(words);
        if(lcp != temp) {
          for(int i=0 ;i<temp.size() ;i++) cout<<"\b \b";
          cout<<lcp<<flush;
          temp = lcp;
          onetab = false;
          continue;
        }
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

void iter(string& cmd, bool inPipeline = false) {
  bool append = false , overWrite = false , directop = false, directerr = false;
  string str = "" , errorstr = "";
  
  vector<string> tokens = tokenize(cmd);
  if(tokens.empty()) return;
  int maxIDX = tokens.size();
  
  string outputFilePath = "";
  
  if(!inPipeline) {
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
  }

  if(cmd == "exit") {
    if(histfileEnv) {
      ofstream File(histFilePath.string());
      for(const string& s : HISTORY) {
        File<<s+"\n";
      }
      File.close();
    }
    exit(0);
  }
  else if(tokens[0] == "history") {
    int i = HISTORY.size();
    fs::path p ;
    bool givenLen = false , readMode = false , makeFile = false;

    for(int j=1 ;j<tokens.size() ;j++) {
      if(is_number(tokens[j])) {
        i = i-stoi(tokens[j])+1;
        givenLen = true;
      }
      else if(tokens[j][0] == '-') {
        for(int k=1 ;k<tokens[j].size() ;k++) {
          extensions.push_back(tokens[j][k]);
          if(tokens[j][k] == 'w') makeFile = true;
        }
      }
      else {
        if(fs::exists(fs::path(tokens[j]))) {
          p = fs::path(tokens[j]);
        }
        else if(makeFile) {
          p = createPathTo(tokens[j]);
        }
      }
    }

    if(!givenLen) i = 1;

    for(char c : extensions) {
      if(c == 'r') {
        readMode = true;
        fstream File(p.string());
        string lines;
        while(getline(File,lines)) {
          HISTORY.push_back(lines);
        }
        File.close();
      }
      else if(c == 'w') {
        readMode = true;
        ofstream File(p.string());
        for(const string& s : HISTORY) {
          File<<s+"\n";
        }
        File.close();
      }
      else if(c == 'a') {
        readMode = true;
        ofstream File(p.string(),ios::app);
        for(int j=lastAppend; j<=HISTORY.size(); j++) {
          File<<HISTORY[j-1]<<"\n";
        }
        File.close();
        lastAppend = HISTORY.size() + 1;
      }
    }

    if(!readMode) {
      for(;i<=HISTORY.size();i++) {
        str+=to_string(i)+"  "+HISTORY[i-1]+"\n";
      }
    }
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
  else if(tokens[0] == "wc") {
    string text = "";
    for(int i=1 ;i<tokens.size() ;i++) {
      if(tokens[i][0] == '-') {
        for(int j=1 ;j<tokens[i].size() ;j++) {
          extensions.push_back(tokens[i][j]);
        }
      }
      else if(fs::exists(fs::path(tokens[i]))) {
        fstream textFile(tokens[i]);
        string lines;
        while(getline(textFile,lines)) text+=lines+"\n";
        textFile.close();
      }
    }

    int lines = count(text.begin() ,text.end() ,'\n');
    int words = 0;
    string temp = "";
    for(char c : text) {
      if(c == ' ') {
        if(temp.size()) words++;
        temp = "";
      }
      else {
        temp += c;
      }
    }
    if(temp.size()) words++;

    int bytes = text.size()-1;
    if(extensions.empty()) {
      str = "\t"+to_string(lines)+"\t"+to_string(words)+"\t"+to_string(bytes)+'\n';
    }
    else {
      bool l = false , w = false , s = false;
      for(char c : extensions) {
        if(c == 'l') l = true;
        else if(c == 'w') w = true;
        else if(c == 's') s = true;
      }

      if(l) str += "\t"+to_string(lines)+"\t";
      if(w) str += "\t"+to_string(words)+"\t";
      if(s) str += "\t"+to_string(bytes)+"\t";
      str += '\n';
    }
  }
  else if(tokens[0] == "echo") {
    for(int i=1 ;i<maxIDX ;i++) {
      string output = tokens[i];
      // Handle \n escape sequences
      size_t pos = 0;
      while((pos = output.find("\\n", pos)) != string::npos) {
        output.replace(pos, 2, "\n");
        pos += 1;
      }
      str += output;
      if(i < maxIDX-1) str+=" ";
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
      return;
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

  if(inPipeline || (!append && !overWrite)) {
    if(str.size()) {
      cout<<str;
    }
    if(errorstr.size()) {
      cerr<<errorstr;
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

void executeCommand(string& cmd) {
  vector<string> tokens = tokenize(cmd);
  if(tokens.empty()) exit(1);

  if(commands[tokens[0]] == "sh") {
    iter(cmd, true);
    exit(0);
  }

  fs::path isExec = checkExec(tokens[0]);

  if(isExec.empty()) {
    cerr << cmd << ": command not found\n";
    exit(1);
  }

  vector<char*> args;
  for(auto& t : tokens) {
    args.push_back(const_cast<char*>(t.c_str()));
  }
  args.push_back(NULL);

  execvp(isExec.string().c_str(), args.data());

  cerr << "Failed to execute " << cmd << "\n";
  exit(1);
}

void executePipeline(vector<string>& cmdsPiped) {
  int numPipes = cmdsPiped.size() - 1;
  int pipefds[numPipes][2];

  for(int i = 0; i < numPipes; i++) {
    if(pipe(pipefds[i]) < 0) {
      cerr << "Pipe creation failed\n";
      return;
    }
  }
  
  vector<pid_t> pids;
  
  for(int i = 0; i < cmdsPiped.size(); i++) {
    pid_t pid = fork();
  
    if(pid == 0) {
      if(i > 0) {
        dup2(pipefds[i-1][0], STDIN_FILENO);
      }
      
      if(i < numPipes) {
        dup2(pipefds[i][1], STDOUT_FILENO);
      }
      
      for(int j = 0; j < numPipes; j++) {
        close(pipefds[j][0]);
        close(pipefds[j][1]);
      }
      
      executeCommand(cmdsPiped[i]);
      exit(0);
    }
    else { 
      pids.push_back(pid);
    }
  }
  
  for(int i = 0; i < numPipes; i++) {
    close(pipefds[i][0]);
    close(pipefds[i][1]);
  }
  
  for(pid_t pid : pids) {
    waitpid(pid, NULL, 0);
  }    
}

int main() {
  cout<<unitbuf;
  cerr<<unitbuf;

  for(const string& str : builtins) commands[str] = "sh";
  for(const string& str : defaultcmds) checkAutoCompletion->insert(str);
  PATH = getenv("PATH");
  HOME = getenv("HOME");

  histfileEnv = getenv("HISTFILE");
  if(histfileEnv != nullptr) {
    HISTORYFILE = string(histfileEnv);
    histFilePath = createPathTo(HISTORYFILE);
    if(fs::exists(histFilePath)) {
      fstream hfile(histFilePath.string());
      string hlines;
      while(getline(hfile,hlines)) {
        HISTORY.push_back(hlines);
      }
      hfile.close();
    }
  }
  
  enableRawMode();
  currHistPtr=0;
  lastAppend = 1;

  while(true) {
    cout << "$ ";
    extensions.clear();
    string cmd ;
    cmd = readCommand();

    HISTORY.emplace_back(cmd);
    currHistPtr = HISTORY.size();
    vector<string> cmdsPiped;
    int st = 0;

    for(int i=0 ;i<cmd.size() ;i++) {
      if(cmd[i] == '|') {
        cmdsPiped.push_back(cmd.substr(st,i-st));
        st = i+1;
      }
    }
    cmdsPiped.push_back(cmd.substr(st));
    
    if(cmdsPiped.size() == 1) {
      iter(cmdsPiped[0]);
    }
    else {
      executePipeline(cmdsPiped);
    }
  }

  return 0;
}