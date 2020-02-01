//
// Created by codetector on 9/2/19.
//

#include "llvm/ADT/STLExtras.h"
#include "MCTargetDesc/TL45MCTargetDesc.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/TargetRegistry.h"

#define DEBUG_TYPE "asm-matcher"

using namespace llvm;

namespace {
struct TL45Operand;

class TL45AsmParser : public MCTargetAsmParser {
  const MCSubtargetInfo &STI;
  MCAsmParser &Parser;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode, OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo, bool MatchingInlineAsm) override;

  bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc, SMLoc &EndLoc) override;

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool ParseDirective(AsmToken DirectiveID) override;

  bool ParseRegister(unsigned &RegNo, OperandVector &Operands);

  bool ParseImmediate(OperandVector &Operands);

  bool ParseOperand(OperandVector &Operands);

  bool ParseSymbolReference(OperandVector &Operands);


  bool parseDirectiveValue(StringRef IDVal, unsigned Size);

#define GET_ASSEMBLER_HEADER
#include "TL45GenAsmMatcher.inc"

public:
  TL45AsmParser(const MCSubtargetInfo &_STI, MCAsmParser &_Parser,
                  const MCInstrInfo &MII, const MCTargetOptions &Options)
          : MCTargetAsmParser(Options, _STI, MII), STI(_STI), Parser(_Parser) {
//            setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
    MCAsmParserExtension::Initialize(Parser);
  }

  unsigned int validateTargetOperandClass(MCParsedAsmOperand &Op, unsigned Kind) override;
};

unsigned int TL45AsmParser::validateTargetOperandClass(MCParsedAsmOperand &Op, unsigned Kind) {
  return MCTargetAsmParser::validateTargetOperandClass(Op, Kind);
}

struct TL45Operand : MCParsedAsmOperand {
  enum KindTy {
    k_Token,
    k_Register,
    k_Immediate,
  } Kind;
  SMLoc StartLoc, EndLoc;

  struct Token {
    const char *Data;
    unsigned Length;
  };

  struct RegOp {
    unsigned Num;
  };

  struct ImmOp {
    const MCExpr *Val;
  };

  union {
    struct Token Tok;
    struct RegOp Reg;
    struct ImmOp Imm;
  };

  TL45Operand(KindTy K) : MCParsedAsmOperand(), Kind(K) {}

public:
  TL45Operand(const TL45Operand &o) : MCParsedAsmOperand() {
    Kind = o.Kind;
    StartLoc = o.StartLoc;
    EndLoc = o.EndLoc;
    switch (Kind) {
      case k_Register:
        Reg = o.Reg;
        break;
      case k_Immediate:
        Imm = o.Imm;
        break;
      case k_Token:
        Tok = o.Tok;
        break;
    }
  }

  /// getStartLoc - Gets location of the first token of this operand
  SMLoc getStartLoc() const override { return StartLoc; }

  /// getEndLoc - Gets location of the last token of this operand
  SMLoc getEndLoc() const override { return EndLoc; }

  unsigned getReg() const override {
    assert (Kind == k_Register && "Invalid access!");
    return Reg.Num;
  }

  const MCExpr *getImm() const {
    assert (Kind == k_Immediate && "Invalid access!");
    return Imm.Val;
  }

  StringRef getToken() const {
    assert (Kind == k_Token && "Invalid access!");
    return StringRef(Tok.Data, Tok.Length);
  }

  // Functions for testing operand type
  bool isReg() const override { return Kind == k_Register; }

  bool isImm() const override { return Kind == k_Immediate; }

  bool isToken() const override { return Kind == k_Token; }

  bool isMem() const override  { return false; }

  bool isImm20() const {
    if (Kind == k_Immediate && Imm.Val->getKind() == MCExpr::ExprKind::Constant) {
      int64_t val = ((const MCConstantExpr *) Imm.Val)->getValue();
      return isInt<20>(val);
    }
    return false;
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    // Add as immediates where possible. Null MCExpr = 0
    if (Expr == nullptr)
      Inst.addOperand(MCOperand::createImm(0));
    else if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm20Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!(Imm20)");
    addExpr(Inst, getImm());
  }

  void print(raw_ostream &OS) const override {
    switch (Kind) {
      case k_Register:
        OS << "Reg<" << Reg.Num << ">";
        break;
      case k_Immediate:
        OS << "Imm<" << ((const MCConstantExpr*)Imm.Val)->getValue() << ">";
        break;
      case k_Token:
        OS << Tok.Data;
        break;
    }
  }

  static std::unique_ptr<TL45Operand> CreateToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<TL45Operand>(k_Token);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static std::unique_ptr<TL45Operand> createReg(unsigned RegNo,
                                                  SMLoc S, SMLoc E) {
    auto Op = std::make_unique<TL45Operand>(k_Register);
    Op->Reg.Num = RegNo;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<TL45Operand> createImm(const MCExpr *Val,
                                                  SMLoc S, SMLoc E) {
    auto Op = std::make_unique<TL45Operand>(k_Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
};
} // namespace (anonymous)

static unsigned MatchRegisterName(StringRef Name); // Body is generate by tableGen

bool
TL45AsmParser::MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode, OperandVector &Operands, MCStreamer &Out,
                                         uint64_t &ErrorInfo, bool MatchingInlineAsm) {
  MCInst Inst;

  if (MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm)) {
    Out.EmitInstruction(Inst, this->STI);
    return false;
  } else {
    return Error(IDLoc, "Error Parsing Instruction"); //TODO Better Info
  }
}

bool TL45AsmParser::ParseRegister(unsigned &RegNo, OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

  switch (getLexer().getKind()) {
    default:
      return true;
    case AsmToken::Identifier:
      const StringRef regName = getLexer().getTok().getIdentifier();
      RegNo = MatchRegisterName(regName);
      if (RegNo != 0) {
        getLexer().Lex();
        Operands.push_back(TL45Operand::createReg(RegNo, S, E));
        return false;
      }

      return true;
  }
}

bool TL45AsmParser::ParseRegister(unsigned &RegNo,
                                    SMLoc &StartLoc, SMLoc &EndLoc) {
  SmallVector<std::unique_ptr<MCParsedAsmOperand>, 1> Operands;
  if (ParseRegister(RegNo, Operands))
    return true;

  TL45Operand &Operand = static_cast<TL45Operand &>(*Operands.front());
  StartLoc = Operand.getStartLoc();
  EndLoc = Operand.getEndLoc();
  return false;
}

bool TL45AsmParser::ParseImmediate(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

  const MCExpr *EVal;
  AsmToken::TokenKind kind = getLexer().getKind();
  switch (kind) {
    default:
      return true;
    case AsmToken::Minus:
    case AsmToken::Integer:
      if (getParser().parseExpression(EVal))
        return true;

      Operands.push_back(TL45Operand::createImm(EVal, S, E));
      return false;
  }
}

bool TL45AsmParser::ParseSymbolReference(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  StringRef Identifier;
  if (Parser.parseIdentifier(Identifier))
    return true;

  SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

  // TODO relocations?

  // Parse a symbol
  MCSymbol *Sym = getContext().getOrCreateSymbol(Identifier);
  const MCExpr *Res = MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, getContext());
  Operands.push_back(TL45Operand::createImm(Res, S, E));
  return false;
}

bool TL45AsmParser::ParseOperand(OperandVector &Operands) {
  // A register operand is always alone.
  switch (getLexer().getKind()) {
    case AsmToken::LParen:
    case AsmToken::RParen:
      Operands.push_back(TL45Operand::CreateToken(
              getLexer().getTok().getString(), getLexer().getTok().getLoc()));
      getLexer().Lex();
      return false;
    default:
      break;
  }
  unsigned RegNo;
  if (!ParseRegister(RegNo, Operands))
    return false;

  // An immediate or expression operand can be alone or followed
  // by a base register.
  SMLoc S = getLexer().getTok().getLoc();
  if (!ParseImmediate(Operands) || !ParseSymbolReference(Operands)) {
    return false;
  }

  return Error(S, "unsupported operand");
}


bool TL45AsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name,
                                       SMLoc NameLoc,
                                       OperandVector &Operands) {
  // First operand is token for instruction
  Operands.push_back(TL45Operand::CreateToken(Name, NameLoc));

  // If there are no more operands, then finish
  if (getLexer().is(AsmToken::EndOfStatement))
    return false;

  // Parse first operand
  if (ParseOperand(Operands))
    return true;

  // Parse until end of statement, consuming commas between operands
  while (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Consume comma token
    if (getLexer().is(AsmToken::Comma)) {
      getLexer().Lex();
      continue;
    }

    // Parse next operand
    if (ParseOperand(Operands))
      return true;
  }

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getLexer().getTok().getLoc(),
                 "unexpected token in operand list");

  return false;
}

bool TL45AsmParser::parseDirectiveValue(StringRef IDVal, unsigned Size) {
  auto parseOp = [&]() -> bool {
    const MCExpr *Value;
    SMLoc ExprLoc = getLexer().getLoc();
    if (getParser().checkForValidSection() || getParser().parseExpression(Value))
      return true;
    // Special case constant expressions to match code generator.
    if (const MCConstantExpr *MCE = dyn_cast<MCConstantExpr>(Value)) {
      assert(Size <= 8 && "Invalid size");
      uint64_t IntValue = MCE->getValue();
      if (!isUIntN(8 * Size, IntValue) && !isIntN(8 * Size, IntValue))
        return Error(ExprLoc, "out of range literal value");
      getStreamer().EmitIntValue(IntValue, Size);
    } else
      getStreamer().EmitValue(Value, Size, ExprLoc);
    return false;
  };

  if (parseMany(parseOp))
    return addErrorSuffix(" in '" + Twine(IDVal) + "' directive");
  return false;
}

bool TL45AsmParser::ParseDirective(AsmToken DirectiveID) {


//
//  const AsmToken &Tok = getLexer().peekTok();
//  if (Tok.getKind() != AsmToken::Identifier) {
//    //getLexer().UnLex(Tok);
//    return true;
//  }
//
//  std::string dir = Tok.getIdentifier();
//
//  if (dir == ".byte") {
//    parseDirectiveValue(dir, 4);
//    return false;
//  }


  return true;
}

extern "C" void LLVMInitializeTL45AsmParser() {
  RegisterMCAsmParser<TL45AsmParser> X(TheTL45Target);
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION

#include "TL45GenAsmMatcher.inc"