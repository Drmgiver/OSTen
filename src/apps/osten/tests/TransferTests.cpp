/* SPDX-License-Identifier: MIT */
#include <Alert.h>
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <Node.h>
#include <Path.h>
#include <RemoveEngine.h>
#include <String.h>
#include <SymLink.h>
#include <TypeConstants.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FileOperations.h"

static BString sFailure;
#define CHECK(expression) do { if (!(expression)) { \
 sFailure.SetToFormat("Line %d: %s", __LINE__, #expression); return false; \
} } while (0)

class Workspace {
public:
 Workspace() {
  char name[] = "/boot/home/Finder tests XXXXXX";
  const char* path = mkdtemp(name);
  if (path != NULL) fRoot = path;
 }
 ~Workspace() {
  if (!fRoot.IsEmpty()) BRemoveEngine().RemoveEntry(fRoot.String());
 }
 BString Path(const char* name) const {
  BString path(fRoot); path << "/" << name; return path;
 }
 bool Directory(const char* name) {
  return create_directory(Path(name).String(), 0755) == B_OK;
 }
 bool Write(const char* name, const char* contents) {
  BFile file(Path(name).String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  return file.InitCheck() == B_OK
   && file.Write(contents, strlen(contents)) == (ssize_t)strlen(contents);
 }
 bool Has(const char* name, const char* contents) const {
  BFile file(Path(name).String(), B_READ_ONLY);
  char buffer[128]; ssize_t count = file.Read(buffer, sizeof(buffer));
  return count == (ssize_t)strlen(contents)
   && memcmp(buffer, contents, count) == 0;
 }
 bool Exists(const char* name) const { return BEntry(Path(name).String()).Exists(); }
 status_t Transfer(const char* source, const char* target, bool copy = false,
  bool preserve = false) {
  entry_ref ref;
  BEntry entry(Path(source).String(), false);
  status_t error = entry.GetRef(&ref);
  if (error != B_OK) return error;
  BDirectory directory(Path(target).String());
  BEntry resolved;
  error = directory.GetEntry(&resolved);
  if (error != B_OK) return error;
  BPath path(&resolved);
  return TransferEntry(ref, directory, path, copy, preserve);
 }
private:
 BString fRoot;
};

static bool MoveFile() {
 Workspace w;
 CHECK(w.Directory("source") && w.Directory("target"));
 CHECK(w.Write("source/item", "move me"));
 CHECK(w.Transfer("source/item", "target") == B_OK);
 CHECK(!w.Exists("source/item") && w.Has("target/item", "move me"));
 return true;
}
static bool CopyAttributes() {
 Workspace w;
 CHECK(w.Directory("source") && w.Directory("target"));
 CHECK(w.Write("source/item", "copy me"));
 BNode source(w.Path("source/item").String());
 const char value[] = "seven labels";
 CHECK(source.WriteAttr("OSTen:test", B_STRING_TYPE, 0, value, sizeof(value))
  == (ssize_t)sizeof(value));
 CHECK(w.Transfer("source/item", "target", true) == B_OK);
 CHECK(w.Has("source/item", "copy me") && w.Has("target/item", "copy me"));
 BNode target(w.Path("target/item").String()); char buffer[64];
 CHECK(target.ReadAttr("OSTen:test", B_STRING_TYPE, 0, buffer, sizeof(buffer))
  == (ssize_t)sizeof(value));
 CHECK(strcmp(buffer, value) == 0);
 return true;
}
static bool Duplicate() {
 Workspace w; CHECK(w.Directory("source") && w.Write("source/item", "original"));
 CHECK(w.Transfer("source/item", "source", true) == B_OK);
 CHECK(w.Transfer("source/item", "source", true) == B_OK);
 CHECK(w.Has("source/item", "original") && w.Has("source/item copy", "original")
  && w.Has("source/item copy 2", "original"));
 return true;
}
static bool SameFolderMove() {
 Workspace w; CHECK(w.Directory("source") && w.Write("source/item", "original"));
 CHECK(w.Transfer("source/item", "source") == B_OK);
 CHECK(w.Has("source/item", "original")); return true;
}
static bool TrashCollision() {
 Workspace w; CHECK(w.Directory("source") && w.Directory("trash"));
 CHECK(w.Write("source/item", "new") && w.Write("trash/item", "old"));
 CHECK(w.Transfer("source/item", "trash", false, true) == B_OK);
 CHECK(w.Has("trash/item", "old") && w.Has("trash/item 2", "new"));
 CHECK(!w.Exists("source/item")); return true;
}
static bool ReplaceFile() {
 Workspace w; CHECK(w.Directory("source") && w.Directory("target"));
 CHECK(w.Write("source/item", "new") && w.Write("target/item", "old"));
 CHECK(w.Transfer("source/item", "target", true) == B_OK);
 CHECK(w.Has("source/item", "new") && w.Has("target/item", "new")); return true;
}
static bool ReplaceDirectory() {
 Workspace w; CHECK(w.Directory("source/item") && w.Directory("target/item"));
 CHECK(w.Write("source/item/new", "new") && w.Write("target/item/old", "old"));
 CHECK(w.Transfer("source/item", "target", true) == B_OK);
 CHECK(w.Has("target/item/new", "new") && !w.Exists("target/item/old"));
 CHECK(w.Has("source/item/new", "new")); return true;
}
static bool MoveReplacement() {
 Workspace w; CHECK(w.Directory("source/item") && w.Directory("target/item"));
 CHECK(w.Write("source/item/new", "new") && w.Write("target/item/old", "old"));
 node_ref before, after;
 CHECK(BEntry(w.Path("source/item").String()).GetNodeRef(&before) == B_OK);
 CHECK(w.Transfer("source/item", "target") == B_OK);
 CHECK(BEntry(w.Path("target/item").String()).GetNodeRef(&after) == B_OK);
 CHECK(before == after && !w.Exists("source/item") && !w.Exists("target/item/old"));
 return true;
}
static bool FailedCopyKeepsDestination() {
 Workspace w; CHECK(w.Directory("source/item") && w.Directory("target/item"));
 CHECK(w.Write("source/item/new", "new") && w.Write("target/item/old", "old"));
 CHECK(mkfifo(w.Path("source/item/unsupported").String(), 0600) == 0);
 CHECK(w.Transfer("source/item", "target", true) == B_NOT_SUPPORTED);
 CHECK(w.Has("target/item/old", "old") && !w.Exists("target/item/new"));
 CHECK(w.Has("source/item/new", "new")); return true;
}
static bool RejectSelf() {
 Workspace w; CHECK(w.Directory("item"));
 CHECK(w.Transfer("item", "item") == B_BAD_VALUE);
 CHECK(w.Transfer("item", "item", true) == B_BAD_VALUE); return true;
}
static bool RejectDescendant() {
 Workspace w; CHECK(w.Directory("item/child"));
 CHECK(w.Transfer("item", "item/child") == B_BAD_VALUE);
 CHECK(w.Transfer("item", "item/child", true) == B_BAD_VALUE); return true;
}
static bool RejectAncestorReplacement() {
 Workspace w; CHECK(w.Directory("item/nested"));
 CHECK(w.Write("item/nested/item", "keep me"));
 CHECK(w.Transfer("item/nested/item", "") == B_BAD_VALUE);
 CHECK(w.Transfer("item/nested/item", "", true) == B_BAD_VALUE);
 CHECK(w.Has("item/nested/item", "keep me")); return true;
}
static bool RejectAliasedDescendant() {
 Workspace w; CHECK(w.Directory("item/child"));
 CHECK(symlink(w.Path("item/child").String(), w.Path("alias").String()) == 0);
 CHECK(w.Transfer("item", "alias", true) == B_BAD_VALUE); return true;
}
static bool CopyDanglingLink() {
 Workspace w; CHECK(w.Directory("source") && w.Directory("target"));
 CHECK(symlink("missing", w.Path("source/link").String()) == 0);
 CHECK(w.Transfer("source/link", "target", true) == B_OK);
 char target[64];
 ssize_t size = readlink(w.Path("target/link").String(), target, sizeof(target));
 CHECK(size == 7 && memcmp(target, "missing", 7) == 0); return true;
}
static bool MissingSource() {
 Workspace w; CHECK(w.Directory("source") && w.Directory("target"));
 CHECK(w.Write("target/item", "keep me"));
 CHECK(w.Transfer("source/item", "target", true) == B_ENTRY_NOT_FOUND);
 CHECK(w.Has("target/item", "keep me")); return true;
}
static bool SiblingPrefix() {
 Workspace w; CHECK(w.Directory("item") && w.Directory("item2"));
 CHECK(w.Write("item/file", "keep me"));
 CHECK(w.Transfer("item", "item2", true) == B_OK);
 CHECK(w.Has("item2/item/file", "keep me")); return true;
}

struct Test { const char* name; bool (*run)(); };
class TestApp : public BApplication {
public:
 TestApp() : BApplication("application/x-vnd.OSTen-TransferTests") {}
 void ReadyToRun() override {
  const Test tests[] = {
   {"move file", MoveFile}, {"copy attributes", CopyAttributes},
   {"duplicate names", Duplicate}, {"same folder move", SameFolderMove},
   {"trash collision", TrashCollision}, {"replace file", ReplaceFile},
   {"replace directory", ReplaceDirectory}, {"move identity", MoveReplacement},
   {"failed copy preserves destination", FailedCopyKeepsDestination},
   {"reject self", RejectSelf}, {"reject descendant", RejectDescendant},
   {"reject ancestor replacement", RejectAncestorReplacement},
   {"reject aliased descendant", RejectAliasedDescendant},
   {"copy dangling link", CopyDanglingLink}, {"missing source", MissingSource},
   {"sibling path prefix", SiblingPrefix}
  };
  BString report; int failures = 0;
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
   bool passed = tests[i].run();
   report << (passed ? "PASS " : "FAIL ") << tests[i].name << "\n";
   if (!passed) { failures++; report << sFailure << "\n"; }
  }
  report << (failures == 0 ? "OSTEN_TRANSFER_TESTS_PASS 16\n"
   : "OSTEN_TRANSFER_TESTS_FAIL\n");
  BFile log("/boot/home/Finder transfer test results.txt",
   B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
  log.Write(report.String(), report.Length());
  // Haiku routes debug output to QEMU's kernel serial log.
  debug_printf("%s", report.String());
  (new BAlert("Finder transfer tests", failures == 0
   ? "PASS: All 16 native Finder transfer tests passed."
   : report.String(), "OK"))->Go();
  PostMessage(B_QUIT_REQUESTED);
 }
};
int main() { TestApp app; app.Run(); return 0; }
