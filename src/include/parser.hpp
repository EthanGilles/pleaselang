#pragma once 
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "arena.hpp"
#include "tokenization.hpp"
#include "token-utils.cpp"
#include "grammar.hpp"

class Parser
{
public:
  inline explicit Parser(std::vector<Token> tokens)
    : m_tokens(std::move(tokens))
    , m_allocator(1024 * 1024 * 4)  {} // 4MB 

  /* TERM -> ID | INT_LIT | (EXPR) */
  std::optional<NodeTerm*> parse_term()
  {
    if (auto int_lit = try_consume(TokenType::INT_LIT))
    {
      auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(int_lit.value());

      auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
      return term;
    }
    else if (auto id = try_consume(TokenType::ID))
    {
      auto term_id = m_allocator.emplace<NodeTermID>(id.value());

      auto term = m_allocator.emplace<NodeTerm>(term_id);
      return term;
    }
    else if (auto op = try_consume(TokenType::LPAREN))
    {
      auto expr = parse_expr();
      if (!expr.has_value())
        error_expected("expression");

      try_consume_err(TokenType::RPAREN);

      auto term_parenth = m_allocator.emplace<NodeTermParenth>(expr.value());

      auto term = m_allocator.emplace<NodeTerm>(term_parenth);
      return term;
    }
    else if (auto character = try_consume(TokenType::CHAR))
    {
      int value = static_cast<int>(character.value().value.value()[0]);
      character.value().value = std::to_string(value);
      auto term_int_lit = m_allocator.emplace<NodeTermIntLit>(character.value());
      auto term = m_allocator.emplace<NodeTerm>(term_int_lit);
      return term;
    }
    else 
      return {};
  }

  std::optional<NodeExpr*> parse_expr(int min_precedence = 0, bool has_cmp = false)
  {
    std::optional<NodeTerm*> term_lhs = parse_term();
    if(!term_lhs.has_value())
      return {};

    auto expr_lhs = m_allocator.emplace<NodeExpr>(term_lhs.value());

    while(true)
    {
      std::optional<Token> curr_token = peek();
      std::optional<int> precedence;
      /* Check precedence */
      if (curr_token.has_value())
      {
        precedence = bin_prec(curr_token->type);
        if (!precedence.has_value() || precedence.value() < min_precedence)
          break;
      }
      else
        break;

      Token op = consume();
      int next_min_precedence = precedence.value() + 1;
      auto expr_rhs = parse_expr(next_min_precedence, has_cmp);
      if (!expr_rhs.has_value())
        error_expected("Expression following an operator");

      auto expr = m_allocator.emplace<NodeBinExpr>();
      auto expr_lhs2 = m_allocator.emplace<NodeExpr>();

      switch (op.type) {
        case TokenType::STAR: {
          expr_lhs2->variant = expr_lhs->variant;
          auto multi = m_allocator.emplace<NodeBinExprMulti>(expr_lhs2, expr_rhs.value());
          expr->variant = multi;
          break;
        }
        case TokenType::PLUS: {
          expr_lhs2->variant = expr_lhs->variant;
          auto add = m_allocator.emplace<NodeBinExprAdd>(expr_lhs2, expr_rhs.value());
          expr->variant = add;
          break;
        }
        case TokenType::MINUS: {
          expr_lhs2->variant = expr_lhs->variant;
          auto sub = m_allocator.emplace<NodeBinExprSub>(expr_lhs2, expr_rhs.value());
          expr->variant = sub;
          break;
        }
        case TokenType::FSLASH: {
          expr_lhs2->variant = expr_lhs->variant;
          auto div = m_allocator.emplace<NodeBinExprDiv>(expr_lhs2, expr_rhs.value());
          expr->variant = div;
          break;
        }
        case TokenType::PERCENT: {
          expr_lhs2->variant = expr_lhs->variant;
          auto mod = m_allocator.emplace<NodeBinExprMod>(expr_lhs2, expr_rhs.value());
          expr->variant = mod;
          break;
        }
        case TokenType::LT: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto lt = m_allocator.emplace<NodeBinCmpLT>(expr_lhs2, expr_rhs.value());
          expr->variant = lt;
          has_cmp = true;
          break;
        }
        case TokenType::GT: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto gt = m_allocator.emplace<NodeBinCmpGT>(expr_lhs2, expr_rhs.value());
          expr->variant = gt;
          has_cmp = true;
          break;
        }
        case TokenType::LTEQ: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto lte = m_allocator.emplace<NodeBinCmpLTEQ>(expr_lhs2, expr_rhs.value());
          expr->variant = lte;
          has_cmp = true;
          break;
        }
        case TokenType::GTEQ: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto gte = m_allocator.emplace<NodeBinCmpGTEQ>(expr_lhs2, expr_rhs.value());
          expr->variant = gte;
          has_cmp = true;
          break;
        }
        case TokenType::NOTEQ: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto noteq = m_allocator.emplace<NodeBinCmpNOTEQ>(expr_lhs2, expr_rhs.value());
          expr->variant = noteq;
          has_cmp = true;
          break;
        }
        case TokenType::DBEQ: {
          if (has_cmp)
            error_invalid("expression. More than one comparison operator");
          expr_lhs2->variant = expr_lhs->variant;
          auto dbeq = m_allocator.emplace<NodeBinCmpDBEQ>(expr_lhs2, expr_rhs.value());
          expr->variant = dbeq;
          has_cmp = true;
          break;
        }
        default: 
          error_invalid("operator: " + to_string(op.type));
      }
      expr_lhs->variant = expr;
    }
    return expr_lhs;
  }

  std::optional<NodeScope*> parse_scope() {
    if(!try_consume(TokenType::LCURLY).has_value())
      return {};

    auto scope = m_allocator.alloc<NodeScope>();
    int last_line = 0;
    /* Parse statements inside scope */
    while (auto stmt = parse_stmt()) 
      scope->stmts.push_back(stmt.value());

    try_consume_err(TokenType::RCURLY);
    return scope;
  }

  std::optional<NodeIfPred*> parse_if_predicate()
  {
    if (try_consume(TokenType::ELSIF))  {
      try_consume_err(TokenType::LPAREN);
      auto elsif = m_allocator.alloc<NodeIfPredElsif>();

      if(auto expr = parse_expr()) 
        elsif->expr = expr.value();
      else 
        error_expected("expression following `eslif`");

      try_consume_err(TokenType::RPAREN);
      if(auto scope = parse_scope()) 
        elsif->scope = scope.value();
      else
       error_expected("scope following `elsif (expr)`");

      elsif->predicate = parse_if_predicate();
      auto predicate = m_allocator.emplace<NodeIfPred>(elsif);
      return predicate;
    }
    else if (try_consume(TokenType::ELSE)) 
    {
      auto else_ = m_allocator.alloc<NodeIfPredElse>();
      if (auto scope = parse_scope()) 
        else_->scope = scope.value();
      else 
        error_expected("scope following `else`");

      auto predicate = m_allocator.emplace<NodeIfPred>(else_);
      return predicate;
    }
    return {};
  }

  std::optional<NodeStmt*> parse_stmt()
  {

    /* PARSE -> 'please' or 'PLEASE' */
    if( (peek().has_value() && peek().value().type == TokenType::PLEASE) || 
        (peek().has_value() && peek().value().type == TokenType::PLEASE_C) )
    {
      auto please = m_allocator.emplace<NodeStmtPlease>();

      if(peek().value().type == TokenType::PLEASE)
        please->value = 1;
      else
        please->value = 2;

      consume();
      auto stmt = m_allocator.emplace<NodeStmt>(please);
      return stmt;
    }


    /* PARSE -> EXIT */
    else if (peek().has_value() && peek().value().type == TokenType::GOODBYE && 
        peek(1).has_value() && peek(1).value().type == TokenType::LPAREN)
    {
      consume(); /* Consume EXIT */
      consume(); /* Consume LPAREN */

      auto stmt_exit = m_allocator.alloc<NodeStmtExit>();

      if (auto node_expr = parse_expr())
        stmt_exit->expr = node_expr.value();
      else
        error_invalid("expression");

      try_consume_err(TokenType::RPAREN);
      try_consume_err(TokenType::SEMI);

      auto stmt = m_allocator.emplace<NodeStmt>();
      stmt->variant = stmt_exit;
      return stmt;
    }


    /* PARSE -> PRINT (EXPR, EXPR, ...) */
    else if (peek().has_value() && peek().value().type == TokenType::PRINT && 
        peek(1).has_value() && peek(1).value().type == TokenType::LPAREN)
    {
      consume(); /* Consume PRINT */
      consume(); /* Consume LPAREN */
      std::vector<NodeExpr*> args;

      while (true)
      {
        if(auto expr = parse_expr())
          args.push_back(expr.value());
        else
          error_expected("expression");

        if(peek().has_value() && peek().value().type == TokenType::COMMA)
          consume();
        else if (peek().has_value() && peek().value().type == TokenType::RPAREN)
        {
          consume();
          break;
        }
      }

      try_consume_err(TokenType::SEMI);
      auto print_stmt = m_allocator.emplace<NodeStmtPrint>();
      print_stmt->args = args;
      auto stmt = m_allocator.emplace<NodeStmt>(print_stmt);
      return stmt;
    }

    /* PARSE -> PRINTN() */
    else if (peek().has_value() && peek().value().type == TokenType::PRINTNL && 
        peek(1).has_value() && peek(1).value().type == TokenType::LPAREN)
    {
      consume(); /* Consume PRINTN */
      consume(); /* Consume LPAREN */
      std::vector<NodeExpr*> args;

      while (true)
      {
        if(auto expr = parse_expr())
          args.push_back(expr.value());
        // OK to be empty.

        if(peek().has_value() && peek().value().type == TokenType::COMMA)
          consume();
        else if (peek().has_value() && peek().value().type == TokenType::RPAREN)
        {
          consume();
          break;
        }
      }

      /* add a newline character at the end */
      Token int_lit = {TokenType::INT_LIT, peek(-1).value().line, "10"};
      auto lit = m_allocator.emplace<NodeTermIntLit>(int_lit);
      auto term = m_allocator.emplace<NodeTerm>(lit);
      auto newline = m_allocator.emplace<NodeExpr>(term);
      args.push_back(newline);

      try_consume_err(TokenType::SEMI);
      auto print_stmt = m_allocator.emplace<NodeStmtPrint>();
      print_stmt->args = args;
      auto stmt = m_allocator.emplace<NodeStmt>(print_stmt);
      return stmt;
    }

    /* PARSE -> SET ID = EXPR */
    else if (peek().has_value() && peek().value().type == TokenType::SET  /* SET */
    && peek(1).has_value() && peek(1).value().type == TokenType::ID       /* ID  */
    && peek(2).has_value() && peek(2).value().type == TokenType::EQUALS)  /*  =  */
    {
      consume();  /* consume SET */
      auto stmt_set = m_allocator.emplace<NodeStmtSet>();
      stmt_set->ID = consume(); /* consume ID */
      consume(); /* consume = */

      if (auto expr = parse_expr())
        stmt_set->expr = expr.value();
      else
        error_invalid("expression");

      try_consume_err(TokenType::SEMI);

      auto stmt = m_allocator.emplace<NodeStmt>();
      stmt->variant = stmt_set;
      return stmt;
    }


    /* PARSE -> RESET ID = [EXPR] */
    else if(peek().has_value() && peek().value().type == TokenType::RESET && 
            peek(1).has_value() && peek(1).value().type == TokenType::ID && 
            peek(2).has_value() && peek(2).value().type == TokenType::EQUALS )
    {
      consume(); // consume RESET
      const auto reset = m_allocator.alloc<NodeStmtReset>();
      reset->ID = consume(); // consume ID 
      consume();

      if(const auto expr = parse_expr()) 
        reset->expr = expr.value();
      else 
        error_invalid("expression");

      try_consume_err(TokenType::SEMI);
      auto stmt = m_allocator.emplace<NodeStmt>(reset);
      return stmt;
    }


    /* PARSE -> SCOPE */
    else if (peek().has_value() && peek().value().type == TokenType::LCURLY)
    {
      if(auto scope = parse_scope())
      {
        auto stmt = m_allocator.emplace<NodeStmt>(scope.value());
        return stmt;
      }
      else
        error_invalid("scope");
    }


    /* PARSE -> WHILE (EXPR) { STMT } */
    else if (auto while_ = try_consume(TokenType::WHILE))
    {
      try_consume_err(TokenType::LPAREN); 

      auto while_stmt = m_allocator.emplace<NodeStmtWhile>();
      if(const auto expr = parse_expr())
        while_stmt->expr = expr.value();
      else
        error_invalid("expression");

      try_consume_err(TokenType::RPAREN);

      if (const auto scope = parse_scope())
        while_stmt->scope = scope.value();
      else
        error_invalid("scope");

      auto stmt = m_allocator.emplace<NodeStmt>(while_stmt);
      return stmt;
    }


    /* PARSE -> IF ( EXPR ) { STMT } */
    else if (auto if_ = try_consume(TokenType::IF)) 
    {
      try_consume_err(TokenType::LPAREN);

      auto if_stmt = m_allocator.emplace<NodeStmtIf>();
      if(const auto expr = parse_expr())
        if_stmt->expr = expr.value();
      else
        error_invalid("expression");

      try_consume_err(TokenType::RPAREN);

      if (const auto scope = parse_scope())
        if_stmt->scope = scope.value();
      else
        error_invalid("scope");

      if_stmt->predicate = parse_if_predicate();

      auto stmt = m_allocator.emplace<NodeStmt>(if_stmt);
      return stmt;
    }
    return {};
  }

  std::optional<NodeProgram> parse_program() 
  {
    NodeProgram program;
    while (peek().has_value())
    {
      if (auto stmt = parse_stmt())
        program.stmts.push_back(stmt.value());
      else
        error_invalid("statement");
    }
    return program;
  }


private:
  [[nodiscard]] inline std::optional<Token> peek(int offset = 0) const 
  {
    if (m_index + offset >= m_tokens.size())
      return {};
    return m_tokens.at(m_index + offset);
  }

  Token consume()
  {
    return m_tokens.at(m_index++);
  }


  Token try_consume_err(TokenType type)
  {
    if(peek().has_value() && peek().value().type == type)
      return consume();
    std::string msg = to_string(type) + " but found " + to_string(peek().value().type);
    error_expected(msg);
    return {};
  }

  std::optional<Token> try_consume(TokenType type)
  {
      if(peek().has_value() && peek().value().type == type)
        return consume();
      return {};
  }

  void error_expected(const std::string &msg)
  {
    std::cerr << "[Parser Error] Expected " << msg << " on line " << peek(-1).value().line << std::endl;
    exit(EXIT_FAILURE);
  }

  void error_invalid(const std::string &msg)
  {
    std::cerr << "[Parser Error] Invalid " << msg << " on line " << peek(-1).value().line << std::endl;
    exit(EXIT_FAILURE);
  }

  const std::vector<Token> m_tokens;
  size_t m_index = 0;
  ArenaAllocator m_allocator;
};
