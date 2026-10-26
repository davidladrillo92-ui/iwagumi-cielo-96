"""Execute production journal throttle with the report-80 low-space condition."""
from pathlib import Path
import subprocess,tempfile
src=Path('src/main.cpp').read_text()
fn=src[src.index('static void flushJournal(){'):src.index('static void persistCritical',src.index('static void flushJournal(){'))]
# Only the function, not adjacent journal helpers.
depth=0;end=None
for i,c in enumerate(fn[fn.index('{'):],fn.index('{')):
 if c=='{':depth+=1
 if c=='}':
  depth-=1
  if depth==0:end=i+1;break
fn=fn[:end]
fixture=r'''
#include "cielo_diagnostic_policy.h"
#include <cassert>
#include <cstddef>
uint32_t now=0;uint32_t millis(){return now;}
bool journalDirty=true,journalHasFlushed=false,curveSaveInProgress=false,low=true;
uint32_t journalLastFlush=0,diagWritesSkippedForSpace=0,savedFailures=184;
struct {unsigned value=0;}journal;
unsigned inspections=0,writes=0;
bool nvsPreserveCurveHeadroom(){inspections++;return low;}
struct {size_t putBytes(const char*,const void*,size_t n){writes++;return n;}
 void putUInt(const char*,unsigned){writes++;}}prefs;
'''
structs=src[src.index("struct StorageWriteCount {"):src.index("static cielo96::nvssafe::Failure lastCurveNvsFailure")]
# Instrumentation declarations lie directly before the existing NVS failure state.
checks=r'''
int main(){
 for(now=0;now<300000;now+=3)flushJournal();
 assert(journalDirty&&diagWritesSkippedForSpace==1&&inspections==1&&writes==0);
 now=300000;flushJournal();assert(diagWritesSkippedForSpace==2&&inspections==2);
 low=false;now=600000;flushJournal();assert(!journalDirty&&writes==2);
 assert(diagnosticWrites.attempts==1&&diagnosticWrites.accepted==1&&restartWrites.attempts==0);
 journalDirty=true;curveSaveInProgress=true;now=900000;flushJournal();
 assert(journalDirty&&diagWritesSkippedForSpace==3&&writes==2&&inspections==3);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+structs+fn+checks);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS actual journal: 100000 low-space loop checkpoints yield one count/read, no flash writes; retries recover after 5min and preserve dirty data')
