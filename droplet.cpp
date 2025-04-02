#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum Token {
    tok_eof = -1,

    tok_def = -2,
    tok_extern = -3,

    tok_identifier = -4,
    tok_number = -5,
};

static std::string IdStr;
static double numVal;

static int gettok(){
    static int lastChar = ' ';

    while (isspace(lastChar)){
        lastChar = getchar();
    }
    
    if(isalpha(lastChar)){
        IdStr = lastChar;
        while ((isalnum(lastChar = getchar()))){
            IdStr += lastChar;
        }

        if(IdStr == "droplet"){
            return tok_def;
        }
        if(IdStr == "extern"){
            return tok_extern;
        }
        return tok_identifier;
    }

    if(isdigit(lastChar) || lastChar == '.'){
        std::string numStr;
        do {
            numStr += lastChar;
            lastChar = getchar();
        }
        while(isdigit(lastChar) || lastChar == '.');
        
        numVal = strtod(numStr.c_str(),0);
        return tok_number;
    }

    if (lastChar == '#'){
        do{
            lastChar = getchar();
        }
        while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');

        if (lastChar != EOF){
            return gettok();
        }

        if (lastChar == EOF){
            return tok_eof;            
        }

        int thisChar = lastChar;
        lastChar = getchar();
        return thisChar;
    }
}

namespace{
    class ExprAST {
        public:
            virtual ~ExprAST() = default;
    };

    class NumberExprAST : public ExprAST{
        double Val;
        public: 
            NumberExprAST(double Val) : Val(Val) {}
    };

    class VariableExprAST : public ExprAST {
        std::string Name;
        public:
        VariableExprAST(const std::string &Name) : Name(Name) {}
    };

    class BinaryExprAST : public ExprAST {
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;

        public: 
            BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    };

    class CallExprAST : public ExprAST{
        std::string callF;
        std::vector<std::unique_ptr<ExprAST>> Args;
        public: 
            CallExprAST(const std::string &callF, std::vector<std::unique_ptr<ExprAST>> Args) : callF(callF), Args(std::move(Args)) {}
    };

    class PrototypeAST{
        std::string Name;
        std::vector<std::string> Args;

        public:
            PrototypeAST(const std::string &Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}

            const std::string &getname() const {return Name;}
    };

    class FunctionAST {
        std::unique_ptr<PrototypeAST> Proto;
        std::unique_ptr<ExprAST> Body;

        public:
            FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body) : Proto(std::move(Proto)), Body(std::move(Body)) {}
    };
}

static int curTok;
static int getNextToken(){
    return curTok = gettok();
}

std::unique_ptr<ExprAST> LogError(const char *Str){
    fprintf(stderr, "Error %s\n", Str);
    return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str){
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr(){
    auto Result = std::make_unique<NumberExprAST>(numVal);
    getNextToken();
    return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST> ParseParenExpr(){
    getNextToken();
    auto V = ParseExpression();
    if (!V){
        return nullptr;
    }

    if (curTok != ')'){
        return LogError("expected ')'");
        getNextToken();
        return V;
    }
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr(){
    std::string IdName = IdStr;

    getNextToken();

    if (curTok != '('){
        return std::make_unique<VariableExprAST>(IdName);
    }

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if(curTok != ')'){
        while (true){
            if (auto Arg = ParseExpression()){
                Args.push_back(std::move(Arg));
            }
            else{
                return nullptr;
            }
            if (curTok == ')'){
                break;
            }
            if(curTok != ','){
                return LogError("Expected ')' or ',' in arg list");
            }
            getNextToken();
        }
    }
    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary(){
    switch (curTok){
        default:
            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
    }
}

static std::map<char, int> BinopPrecendence;

static int GetTokPrecedence(){
    if(!isascii(curTok)){
        return -1;
    }

    int TokPrec = BinopPrecendence[curTok];
    if (TokPrec <= 0){
        return -1;
    }
    return TokPrec;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS){
    while(true){
        int TokPrec = GetTokPrecedence();
        if(TokPrec < ExprPrec){
            return LHS;
        }

        int BinOp = curTok;
        getNextToken();
        auto RHS = ParsePrimary();
        if(!RHS){
            return nullptr;
        }

        int NextPrec = GetTokPrecedence();
        if(TokPrec < NextPrec){
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS){
                return nullptr;
            }
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}


static std::unique_ptr<ExprAST> ParseExpression(){
    auto LHS = ParsePrimary();
    if(!LHS){
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<PrototypeAST> ParsePrototype(){
    if(curTok != tok_identifier){
        return LogErrorP("expected function name in prototype");
    }

    std::string FnName = IdStr;
    getNextToken();

    if(curTok != '('){
        return LogErrorP("expected '(' in prototype");
    }

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier){
        ArgNames.push_back(IdStr);
    }
    if (curTok != ')')
    {
        return LogErrorP("expected ')' in prototype");
    }

    getNextToken();
    
    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition(){
    getNextToken();
    auto Proto = ParsePrototype();
    if(!Proto) return nullptr;

    if(auto E = ParseExpression()){
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern(){
    getNextToken();
    return ParsePrototype();
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr(){
    if(auto E = ParseExpression()){
        auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static void HandleDefinition() {
    if (ParseDefinition()) {
      fprintf(stderr, "Parsed a function definition.\n");
    } else {
      getNextToken();
    }
  }
  
  static void HandleExtern() {
    if (ParseExtern()) {
      fprintf(stderr, "Parsed an extern\n");
    } else {
      getNextToken();
    }
  }
  
  static void HandleTopLevelExpression() {
    if (ParseTopLevelExpr()) {
      fprintf(stderr, "Parsed a top-level expr\n");
    } else {
      getNextToken();
    }
  }

  static void MainLoop() {
    while (true) {
      fprintf(stderr, "ready> ");
      switch (curTok) {
      case tok_eof:
        return;
      case ';': 
        getNextToken();
        break;
      case tok_def:
        HandleDefinition();
        break;
      case tok_extern:
        HandleExtern();
        break;
      default:
        HandleTopLevelExpression();
        break;
      }
    }
  }


int main(){
    BinopPrecendence['<'] = 10;
    BinopPrecendence['>'] = 10;
    BinopPrecendence['+'] = 20;
    BinopPrecendence['-'] = 20;
    BinopPrecendence['*'] = 40;
    BinopPrecendence['/'] = 40;


    fprintf(stderr, "ready>");
    getNextToken();

    MainLoop();
    return 0;
}
