from pathlib import Path
import tempfile, subprocess
s=Path('src/main.cpp').read_text(); fn=s[s.index('static void flushRestartJournal(){'):s.index('static uint32_t savedFailures=0;')]
fixture=r'''
#include "cielo_recovery315.h"
#include "cielo_restart_journal.h"
#include <cassert>
uint32_t now=0;uint32_t millis(){return now;}
const char* restartWriteStatus315="NOT_ATTEMPTED";
bool restartJournalDirty=true,restartJournalWriteFailed315=false,curveSaveInProgress=false,low=true,fail=false;
uint32_t restartJournalSavedAt=0,restartWritesSkippedForSpace=0,restartWrites=0;
cielo96::restarts::Journal restartJournal;
bool nvsOptionalHeadroom(unsigned n){return !low&&n==16;}
size_t trackedJournalWrite(const char*,const void*,size_t n,uint32_t& c){++c;return fail?0:n;}
'''
checks=r'''
int main(){now=300000;flushRestartJournal();assert(restartWrites==0&&restartWritesSkippedForSpace==1&&restartJournalDirty);now+=1799999;flushRestartJournal();assert(restartWritesSkippedForSpace==1);low=false;fail=true;++now;flushRestartJournal();assert(restartWrites==1&&restartJournalDirty);now+=1799999;flushRestartJournal();assert(restartWrites==1);++now;fail=false;flushRestartJournal();assert(restartWrites==2&&!restartJournalDirty);now+=1800000;flushRestartJournal();assert(restartWrites==2);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.cpp';p.write_text(fixture+fn+checks);out=str(Path(d)/'t');subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True);subprocess.run([out],check=True)
print('PASS actual restart journal: low margin, failed write, backoff, eventual success')
