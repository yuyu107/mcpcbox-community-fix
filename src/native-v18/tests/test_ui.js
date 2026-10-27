const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('native-v18/official-ui.html','utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const dom={};for(const id of ['parallel','refresh','notice','type','sort','search','list','page','prev','next','task','tasktitle','taskmsg','taskerror','connection','fill','stats','directory','cancel','taskclose','probe','help','taskdetails','noticebox','connectionbox'])dom[id]={style:{},value:'',innerHTML:'',disabled:false,setAttribute:function(k,v){this[k]=v;}};
dom.parallel.value='8';dom.type.value='release';dom.sort.value='desc';
const context={document:{getElementById:id=>dom[id]},setInterval:()=>{},console};vm.createContext(context);vm.runInContext(script,context);
const manifest=JSON.parse(fs.readFileSync('native-v18/tests/official-manifest.json','utf8'));
let fail=false,requests=[],state='downloading',taskId=1;
context.api=(method,path,done)=>{requests.push([method,path]);if(fail)return done('network failed');if(path.indexOf('/api/versions')===0)return done(null,{versions:manifest.versions,latest:manifest.latest,architecture:'64'});if(path==='/api/status')return done(null,{state,taskId,id:'1.8.9',doneFiles:50,totalFiles:100,doneBytes:512,totalBytes:1024,directory:'D:\\MCBox\\download\\game\\1.8.9\\.minecraft',cacheHits:12,localHits:4,reusedBytes:1024,java:'8',message:'downloading'});return done(null,{accepted:true});};
context.loadVersions(false);assert(dom.notice.innerHTML.includes('920'));assert((dom.list.innerHTML.match(/class="item"/g)||[]).length===40,'pagination');
dom.search.value='1.8.9';context.changed();assert((dom.list.innerHTML.match(/class="item"/g)||[]).length===1);assert(dom.list.innerHTML.includes('data-id="1.8.9"'));
context.download('1.8.9');assert(requests.some(x=>x[0]==='POST'&&x[1]==='/api/download?id=1.8.9&parallel=8'));assert(context.active);assert(dom.fill.style.width==='50.0%');assert(dom.stats.innerHTML.includes('Java 8'));assert(dom.stats.innerHTML.includes('缓存复用 12'));assert(dom.stats.innerHTML.includes('已安装复用 4'));
assert(dom.list.innerHTML.includes('下载中…'));assert.equal(dom.taskclose.style.display,'none');context.dismissTask();assert.equal(dom.task.style.display,'block');
for(const terminal of ['complete','failed','cancelled']){state=terminal;context.poll();assert.equal(dom.taskclose.style.display,'inline-block');context.dismissTask();assert.equal(dom.task.style.display,'none');context.poll();assert.equal(dom.task.style.display,'none','terminal dismissal must survive polling');taskId++;context.poll();assert.equal(dom.task.style.display,'block','another run of the same version must reappear');}
state='downloading';context.poll();
// Closing a completed task is remembered across a page reload in this session.
state='complete';context.poll();context.dismissTask();let saved=context.dismissedTask;assert(saved);assert.equal(dom.task.style.display,'none');state='downloading';taskId++;context.poll();
dom.help.style.display='none';context.toggle('help',dom.probe);assert.equal(dom.help.style.display,'block');assert.equal(dom.probe['aria-expanded'],'true');context.toggle('help',dom.probe);assert.equal(dom.help.style.display,'none');
dom.search.value='';dom.type.value='snapshot';context.changed();assert(!dom.list.innerHTML.includes('最新正式版'));assert(dom.page.innerHTML.includes('页'));
context.pageNo=1;context.render();assert(dom.page.innerHTML.startsWith('2 /'));
context.active=false;context.download('1.8.9',true);assert(requests.some(x=>x[1]==='/api/test-client?id=1.8.9&parallel=8'));context.cancelDownload();assert(requests.some(x=>x[1]==='/api/cancel'));
fail=true;context.loadVersions(true);assert(dom.notice.className==='error');
// A delayed status from before submission must not reset the new task.
context.active=false;let oldApi=context.api,oldPoll,submission;context.api=(m,p,cb)=>{if(p==='/api/status')oldPoll=cb;else submission=cb;};context.poll();context.download('1.8.9');oldPoll(null,{state:'idle'});assert(context.active&&context.submitting);assert(dom.tasktitle.innerHTML.includes('准备中'));submission(null,{});oldPoll(null,{state:'preparing',id:'1.8.9',taskId:99});assert(context.active);assert(dom.list.innerHTML.includes('准备中…')||context.buttonLabel('1.8.9')==='准备中…');context.api=oldApi;
context.active=false;context.api=(m,p,cb)=>p==='/api/status'?cb(null,{state:'complete',id:'old',taskId:1}):cb('submission rejected');context.download('1.8.9');assert(dom.taskerror.innerHTML.includes('submission rejected'));context.dismissTask();context.poll();assert.equal(dom.task.style.display,'none','an old terminal status cannot overwrite a dismissed submission failure');context.api=oldApi;
assert(!/\b(let|const|Promise|fetch)\b|=>|\.includes\(/.test(script));
console.log('PASS: stateful download buttons, all terminal-state dismissal survives polling, distinct same-version task IDs, stale status/submission failure handling, collapsed help; official catalog, release/snapshot filters, search, pagination, download POST/version ID, task progress, cancellation, network errors, classic JS syntax. Browser rendering/IE remain untested.');
