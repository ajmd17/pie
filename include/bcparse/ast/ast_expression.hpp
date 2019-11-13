#pragma once

#include <bcparse/ast/ast_statement.hpp>

#include <bcparse/emit/obj_loc.hpp>

namespace bcparse {
  class AstExpression : public AstStatement {
  public:
    AstExpression(const SourceLocation &location);

    virtual AstExpression *getValueOf();
    virtual std::string toString() const;

    inline const ObjLoc &getObjLoc() const { return m_objLoc; }

  protected:
    // set during walk...
    ObjLoc m_objLoc;
  };
}
