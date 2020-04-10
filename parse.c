#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "9cc.h"


extern Token *token;
extern Node *code[100];
extern char *user_input;
LVar *locals;


void error_at(char *loc, char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);

	int pos = loc - user_input;
	fprintf(stderr, "%s\n", user_input);
	fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
	fprintf(stderr, "^ ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
}

// エラー報告のための関数
// printfと同じ引数を取る
void error(char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
}

// 次のトークンが期待している記号の時は、トークンを１つ読み進めて
// trueを返す。それ以外はfalseを返す。
bool consume(char *op){
	if(token->kind != TK_RESERVED 
		|| strlen(op) != token->len
		|| memcmp(token->str, op, token->len)){
		return false;
	}
	token = token->next;
	return true;
}

Token *consume_ident(){
    if(token->kind != TK_IDENT){
        return NULL;
    }
    Token *tok_ident = token;
    token = token->next;
    return tok_ident;
}

// 次のトークンが期待している記号の時は、トークンを１つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *s){
	if(token->kind != TK_RESERVED 
		|| strlen(s) != token->len
		|| memcmp(token->str, s, token->len)){
		error_at(token->str, "'%s'ではありません。", s);
	}
	token = token->next;
}

// 次のトークンが数値の場合、トークンを１つ読み進めてその数値を返す。
// それ以外はエラーを報告する。
int expect_number(){
	if(token->kind != TK_NUM){
		error_at(token->str, "数ではありません。");
	}
	int val = token->val;
	token = token->next;
	return val;
}

// 次のトークンが識別子の場合、トークンを１つ読み進める。
// それ以外はエラーを報告する。
char *expect_ident(void){
	if(token->kind != TK_IDENT){
		error_at(token->str, "識別子が来るはずです。");
	}
	char *s = strndup(token->str, token->len);
	token = token->next;
	return s;
}

bool at_eof(){
	return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurにつなげる
Token *new_token(TokenKind kind, Token *cur, char *str, int len){
	Token *tok = calloc(1, sizeof(Token));
	tok->kind = kind;
	tok->str = str;
	tok->len = len;
	cur->next = tok;
	return tok;
}

// 特定の文字列で始まっているか確認
bool startswith(char *p, char *q){
	return memcmp(p, q, strlen(q)) == 0;
}

// 変数を名前で検索する。ない場合はNULL。
LVar *find_lvar(Token *tok){
	for(LVar *var = locals; var; var = var->next){
		if(var->len == tok->len && !memcmp(tok->str, var->name, var->len)){
			return var;
		}
	}
	return NULL;
}

int is_alnum(char c){
	return ('a' <= c && c <= 'z') ||
		   ('A' <= c && c <= 'Z') ||
		   ('0' <= c && c <= '9') ||
		   (c == '_');
}

char* strndup(char *str, size_t len) {

    char *buffer = malloc(len + 1);
    memcpy(buffer, str, len);
    buffer[len] = '\0';

    return buffer;
}

char *starts_with_reserved(char *p){

	// 予約語チェック
	static char *kw[] = {
		"return", "if", "else", "while", "for", "int"
	};
	for(int i = 0; i < sizeof(kw) / sizeof(*kw); i++){
		int len = strlen(kw[i]);
		if(startswith(p, kw[i]) && !is_alnum(p[len])){
			return kw[i];
		}
	}

	// ２文字演算子チェック
	static char *ops[] = {
		"==", "!=", "<=", ">="
	};
	for(int i = 0; i < sizeof(ops) / sizeof(*ops); i++){
		if(startswith(p, ops[i])){
			return ops[i];
		}
	}

	return NULL;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p){
	Token head;
	head.next = NULL;
	Token *cur = &head;

	while(*p){

		// 空白文字、改行をスキップ
		if(isspace(*p) || *p == '\n'){
			p++;
			continue;
		}

		char *kw = starts_with_reserved(p);
		if(kw){
			int len = strlen(kw);
			cur = new_token(TK_RESERVED, cur, p, len);
			p += len;
			continue;
		}

		if(strchr("+-*/()<>;={}&,", *p)){
			cur = new_token(TK_RESERVED, cur, p++, 1);
			continue;
		}

        if(isalpha(*p)){
			char *p_begin = p;
			int len = 0;
			while(is_alnum(*p)){
				len++;
				p++;
			}
            cur = new_token(TK_IDENT, cur, p_begin, len);
            continue;
        }

		if(isdigit(*p)){
			cur = new_token(TK_NUM, cur, p, 0);
			char *p_begin = p;
			cur->val = strtol(p, &p, 10);
			cur->len = p - p_begin;
			continue;
		}

		error_at(p, "トークナイズできません。");
	}

	new_token(TK_EOF, cur, p, 0);
	return head.next;
}

Node *new_node(NodeKind kind){
	Node *node = calloc(1, sizeof(Node));
	node->kind = kind;
	return node;
}

Node *new_node_binary(NodeKind kind, Node *lhs, Node *rhs){
	Node *node = calloc(1, sizeof(Node));
	node->kind = kind;
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

Node *new_node_unary(NodeKind kind, Node *unary){
	Node *node = calloc(1, sizeof(Node));
	node->kind = kind;
	node->lhs = unary;
	return node;
}

Node *new_node_ifelse(Node *cond, Node *then, Node *els){
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_IF;
	node->cond = cond;
	node->then = then;
	node->els  = els;
	return node;
}

Node *new_node_while(Node *cond, Node *then){
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_WHILE;
	node->cond = cond;
	node->then = then;
	return node;
}

Node *new_node_for(Node *init, Node *cond, Node *inc, Node *then){
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_FOR;
	node->init = init;
	node->cond = cond;
	node->inc  = inc;
	node->then = then;
	return node;
}

Node *new_node_block(Node *body){
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_BLOCK;
	node->body = body;
	return node;
}

Node *new_node_num(int val){
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_NUM;
	node->val = val;
	return node;
}

Node *new_node_lvar(LVar *var){
	Node *node = new_node(ND_LVAR);
	node->lvar = var;
	return node;
}

LVar *new_lvar(char *name, Type *ty){
	LVar *var = calloc(1, sizeof(LVar));
	var->name = name;
	var->len  = strlen(name);
	var->ty   = ty;
	var->next = locals;
	locals = var;
	return var;
}

LVar *read_func_params(void){
	if(consume(")")) return NULL;

	Type *ty = basetype();
	LVar *params = new_lvar(expect_ident(), ty);

	while(!consume(")")){
		expect(",");
		Type *ty = basetype();
		params = new_lvar(expect_ident(), ty);
	}

	return params;
}

Function *program(){
	Function head = {};
	Function *cur = &head;

    while(!at_eof()){
        cur->next = function();
		cur = cur->next;
    }

    return head.next;
}

Type *basetype(){
	expect("int");
	Type *ty = int_type;
	return ty;
}

Function *function(){
	locals = NULL;

	// Function 構造体を生成
	Function *fn = calloc(1, sizeof(Function));

	// 関数名をパース
	basetype();
	fn->name = expect_ident();

	// 引数をパース
	expect("(");
	fn->params = read_func_params();

	// ブロックをパース
	expect("{");

	Node head = {};
	Node *cur = &head;

	while(!consume("}")){
		cur->next = stmt();
		cur = cur->next;
	}

	fn->node = head.next;
	fn->locals = locals;

	return fn;
}

Node *declaration(){
	Type *ty = basetype();
	LVar *var = new_lvar(expect_ident(), ty);

	if(consume(";")){
		return new_node(ND_NULL);
	}

	expect("=");
	Node *lhs = new_node_lvar(var);
	Node *rhs = expr();
	expect(";");

	Node *node = new_node_binary(ND_ASSIGN, lhs, rhs);
}

Node *stmt(){
    Node *node;

	if(consume("return")){
		Node *node = new_node_unary(ND_RETURN, expr());
		expect(";");
		return node;
	}
	else if(consume("if")){
		Node *cond, *then, *els;
		expect("(");
		cond = expr();
		expect(")");
		then = stmt();
		if(consume("else")){
			els = stmt();
		}
		return new_node_ifelse(cond, then, els);
	}
	else if(consume("while")){
		expect("(");
		node = expr();
		expect(")");
		return new_node_while(node, stmt());
	}
	else if(consume("for")){
		Node *init = NULL, *cond = NULL, *inc = NULL;
		expect("(");
		if(strncmp(token->str, ";", 1) != 0){
			init = expr();
		}
		expect(";");
		if(strncmp(token->str, ";", 1) != 0){
			cond = expr();
		}
		expect(";");
		if(strncmp(token->str, ")", 1) != 0){
			inc = expr();
		}
		expect(")");
		return new_node_for(init, cond, inc, stmt());
	}
	else if(consume("{")){
		Node head = {}; // define & initialize
		Node *cur = new_node(ND_BLOCK);
		head.next = cur;
		while(!consume("}")){
			cur->body = stmt();
			cur->next = new_node(ND_BLOCK);
			cur = cur->next;
		}
		cur->next = NULL;

		return head.next;
	}
	else if(!strncmp(token->str, "int", token->len)){
		return declaration();
	}
	else{
		node = expr();
	}

    expect(";");
    return node;
}

Node *expr(){
	return assign();
}

Node *assign(){
    Node *node = equality();

    if(consume("=")){
         node = new_node_binary(ND_ASSIGN, node, assign());
    }
    return node;
}

Node *equality(){
	Node *node = relational();

	for(;;){
		if(consume("==")){
			node = new_node_binary(ND_EQ, node, relational());
		}
		else if(consume("!=")){
			node = new_node_binary(ND_NE, node, relational());
		}
		else{
			return node;
		}
	}
}

Node *relational(){
	Node *node = add();

	for(;;){
		if(consume("<")){
			node = new_node_binary(ND_LT, node, add());
		}
		else if(consume(">")){
			node = new_node_binary(ND_LT, add(), node);
		}
		else if(consume("<=")){
			node = new_node_binary(ND_LE, node, add());
		}
		else if(consume(">=")){
			node = new_node_binary(ND_LE, add(), node);
		}
		else{
			return node;
		}
	}
}

Node *add(){
	Node *node = mul();

	for(;;){
		if(consume("+")){
			node = new_node_binary(ND_ADD, node, mul());
		}
		else if(consume("-")){
			node = new_node_binary(ND_SUB, node, mul());
		}
		else{
			return node;
		}
	}
}

Node *mul(){
	Node *node = unary();

	for(;;){
		if(consume("*")){
			node = new_node_binary(ND_MUL, node, unary());
		}
		else if(consume("/")){
			node = new_node_binary(ND_DIV, node, unary());
		}
		else{
			return node;
		}
	}
}

Node *unary(){
	if(consume("+")){
		return primary();
	}
	if(consume("-")){
		return new_node_binary(ND_SUB, new_node_num(0), primary());
	}
	if(consume("*")){
		return new_node_unary(ND_DEREF, unary());
	}
	if(consume("&")){
		return new_node_unary(ND_ADDR, unary());
	}
	return primary();
}

Node *primary(){
	if(consume("(")){
		Node *node = expr();
		expect(")");
		return node;
	}

    Token *tok = consume_ident();
    if(tok){
		// function call
		if(consume("(")){
			Node *node = new_node(ND_FUNCCALL);
			node->funcname = strndup(tok->str, tok->len);
			if(consume(")")){
				node->args = NULL;
			}
			else{
				Node *head = assign();
				Node *cur = head;
				while(consume(",")){
					cur->next = assign();
					cur = cur->next;
				}
				expect(")");
				node->args = head;
			}
			return node;
		}
		// local variable
		else{
			Node *node = calloc(1, sizeof(Node));
			node->kind = ND_LVAR;

			LVar *lvar = find_lvar(tok);
			if(!lvar){
				lvar = calloc(1, sizeof(LVar));
				lvar->next = locals;
				lvar->name = tok->str;
				lvar->len = tok->len;
				locals = lvar;
			}
			node->lvar = lvar;
			return node;
		}
    }

	return new_node_num(expect_number());
}