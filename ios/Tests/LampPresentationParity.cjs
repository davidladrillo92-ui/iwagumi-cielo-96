// Compare fixtures with the actual web formatter, not a duplicate JS implementation.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('src/cielo_ui.cpp','utf8');
const start=source.indexOf('function lampPresentation('),end=source.indexOf('function renderLampStates(',start);
assert(start>=0&&end>start,'Web lamp formatter missing');
const present=vm.runInNewContext(source.slice(start,end)+';lampPresentation');
const fixtures=JSON.parse(fs.readFileSync('ios/Tests/LampPresentationFixtures.json','utf8'));
for(const fixture of fixtures){
 const value=present(fixture.state,fixture.pv?'pez':'chi',fixture.fresh);
 const actual=JSON.parse(JSON.stringify({label:value.label,values:value.values}));
 assert.deepEqual(actual,fixture.expected,fixture.name);
}
console.log(`PASS ${fixtures.length} lamp-state fixtures against the actual firmware web formatter`);
