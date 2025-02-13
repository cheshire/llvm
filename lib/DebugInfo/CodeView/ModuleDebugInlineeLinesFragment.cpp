//===- ModuleDebugInlineeLineFragment.cpp ------------------------*- C++-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/DebugInfo/CodeView/ModuleDebugInlineeLinesFragment.h"

#include "llvm/DebugInfo/CodeView/CodeViewError.h"
#include "llvm/DebugInfo/CodeView/ModuleDebugFragmentRecord.h"

using namespace llvm;
using namespace llvm::codeview;

Error VarStreamArrayExtractor<InlineeSourceLine>::extract(
    BinaryStreamRef Stream, uint32_t &Len, InlineeSourceLine &Item,
    ContextType *Fragment) {
  BinaryStreamReader Reader(Stream);

  if (auto EC = Reader.readObject(Item.Header))
    return EC;

  if (Fragment->hasExtraFiles()) {
    uint32_t ExtraFileCount;
    if (auto EC = Reader.readInteger(ExtraFileCount))
      return EC;
    if (auto EC = Reader.readArray(Item.ExtraFiles, ExtraFileCount))
      return EC;
  }

  Len = Reader.getOffset();
  return Error::success();
}

ModuleDebugInlineeLineFragmentRef::ModuleDebugInlineeLineFragmentRef()
    : ModuleDebugFragmentRef(ModuleDebugFragmentKind::InlineeLines) {}

Error ModuleDebugInlineeLineFragmentRef::initialize(BinaryStreamReader Reader) {
  if (auto EC = Reader.readEnum(Signature))
    return EC;

  if (auto EC = Reader.readArray(Lines, Reader.bytesRemaining(), this))
    return EC;

  assert(Reader.bytesRemaining() == 0);
  return Error::success();
}

bool ModuleDebugInlineeLineFragmentRef::hasExtraFiles() const {
  return Signature == InlineeLinesSignature::ExtraFiles;
}

ModuleDebugInlineeLineFragment::ModuleDebugInlineeLineFragment(
    bool HasExtraFiles)
    : ModuleDebugFragment(ModuleDebugFragmentKind::InlineeLines),
      HasExtraFiles(HasExtraFiles) {}

uint32_t ModuleDebugInlineeLineFragment::calculateSerializedLength() {
  // 4 bytes for the signature
  uint32_t Size = sizeof(InlineeLinesSignature);

  // one header for each entry.
  Size += Entries.size() * sizeof(InlineeSourceLineHeader);
  if (HasExtraFiles) {
    // If extra files are enabled, one count for each entry.
    Size += Entries.size() * sizeof(uint32_t);

    // And one file id for each file.
    Size += ExtraFileCount * sizeof(uint32_t);
  }
  assert(Size % 4 == 0);
  return Size;
}

Error ModuleDebugInlineeLineFragment::commit(BinaryStreamWriter &Writer) {
  InlineeLinesSignature Sig = InlineeLinesSignature::Normal;
  if (HasExtraFiles)
    Sig = InlineeLinesSignature::ExtraFiles;

  if (auto EC = Writer.writeEnum(Sig))
    return EC;

  for (const auto &E : Entries) {
    if (auto EC = Writer.writeObject(E.Header))
      return EC;

    if (!HasExtraFiles)
      continue;

    if (auto EC = Writer.writeInteger<uint32_t>(E.ExtraFiles.size()))
      return EC;
    if (auto EC = Writer.writeArray(makeArrayRef(E.ExtraFiles)))
      return EC;
  }

  return Error::success();
}

void ModuleDebugInlineeLineFragment::addExtraFile(uint32_t FileOffset) {
  auto &Entry = Entries.back();
  Entry.ExtraFiles.push_back(ulittle32_t(FileOffset));
  ++ExtraFileCount;
}

void ModuleDebugInlineeLineFragment::addInlineSite(TypeIndex FuncId,
                                                   uint32_t FileOffset,
                                                   uint32_t SourceLine) {
  Entries.emplace_back();
  auto &Entry = Entries.back();
  Entry.Header.FileID = FileOffset;
  Entry.Header.SourceLineNum = SourceLine;
  Entry.Header.Inlinee = FuncId;
}
