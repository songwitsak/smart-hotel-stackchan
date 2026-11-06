import http from 'node:http';
import crypto from 'node:crypto';

const PORT = Number(process.env.PORT || 8080);
const DEVICE_TOKEN = process.env.DEVICE_TOKEN || '';
const OPENAI_API_KEY = process.env.OPENAI_API_KEY || '';
const SUMANEE_URL = process.env.SUMANEE_URL || 'https://sumaneeplatform.witmorning.com/webhook/sumanee-voice-stream-b8f24a91';
const MAX_BODY = 160044;

function extractText(body) {
  const get = (obj) => {
    if (typeof obj === 'string') return obj;
    if (Array.isArray(obj)) return obj.map(get).join('');
    if (obj && typeof obj === 'object') {
      for (const key of ['output','text','delta','content','message']) {
        if (obj[key] != null) { const v = get(obj[key]); if (v) return v; }
      }
    }
    return '';
  };
  try { const t = get(JSON.parse(body)); if (t) return t.trim(); } catch {}
  const parts = [];
  for (const line of body.split(/\r?\n/)) {
    const s = line.trim();
    if (!s || s === '[DONE]' || /^(event|id|retry):/.test(s)) continue;
    const data = s.startsWith('data:') ? s.slice(5).trim() : s;
    if (!data || data === '[DONE]') continue;
    try { parts.push(get(JSON.parse(data))); }
    catch { if (!s.startsWith('data:')) parts.push(data); }
  }
  return parts.join('').trim();
}
function send(res, code, msg) {
  res.writeHead(code, {'content-type':'application/json','cache-control':'no-store'});
  res.end(JSON.stringify({error:msg}));
}
function validWav(buf) {
  if (buf.length < 44 || buf.length > MAX_BODY ||
      buf.toString('ascii',0,4)!=='RIFF' || buf.toString('ascii',8,12)!=='WAVE') return false;
  let pos = 12, rate = 0, channels = 0, bits = 0, format = 0, frames = 0;
  while (pos + 8 <= buf.length) {
    const size = buf.readUInt32LE(pos+4);
    if (size > buf.length-pos-8) return false;
    const tag = buf.toString('ascii',pos,pos+4);
    if (tag==='fmt ' && size>=16) {
      format=buf.readUInt16LE(pos+8);channels=buf.readUInt16LE(pos+10);
      rate=buf.readUInt32LE(pos+12);bits=buf.readUInt16LE(pos+22);
    }
    if (tag==='data') frames=size/2;
    pos+=8+size+(size&1);
  }
  return format===1 && rate===16000 && channels===1 && bits===16 && frames>0 && frames<=80000;
}
async function openAi(path, options) {
  const response=await fetch('https://api.openai.com/v1/'+path,{
    ...options, headers:{Authorization:'Bearer '+OPENAI_API_KEY,...options.headers},
    signal:AbortSignal.timeout(65000)
  });
  if (!response.ok) throw new Error('OpenAI upstream '+response.status);
  return response;
}
const server=http.createServer(async(req,res)=>{
  if(req.method==='GET' && req.url==='/health'){
    res.writeHead(200,{'content-type':'application/json'});res.end('{"ok":true}');return;
  }
  if(req.method!=='POST' || req.url!=='/v1/voice'){send(res,404,'Not found');return;}
  if(!DEVICE_TOKEN || !OPENAI_API_KEY){send(res,503,'Voice bridge credentials not configured');return;}
  const auth=req.headers.authorization||'';
  const expected='Bearer '+DEVICE_TOKEN;
  const a=Buffer.from(auth), b=Buffer.from(expected);
  if(a.length!==b.length || !crypto.timingSafeEqual(a,b)){send(res,401,'Unauthorized');return;}
  const session=req.headers['x-session-id']||'';
  if(typeof session!=='string'||!/^[a-zA-Z0-9_-]{8,100}$/.test(session)){send(res,400,'Invalid session');return;}
  if((req.headers['content-type']||'').split(';')[0].trim()!=='audio/wav'){send(res,415,'Expected audio/wav');return;}
  const chunks=[];let bytes=0;
  try {
    for await(const chunk of req){
      bytes+=chunk.length;
      if(bytes>MAX_BODY){send(res,413,'Audio too large');return;}
      chunks.push(chunk);
    }
    const wav=Buffer.concat(chunks);
    if(!validWav(wav)){send(res,400,'Expected 16kHz mono PCM16 WAV (max 5 sec)');return;}
    const form=new FormData();
    form.append('model','gpt-4o-mini-transcribe');
    form.append('language','th');
    form.append('file',new Blob([wav],{type:'audio/wav'}),'recording.wav');
    const transcription=await openAi('audio/transcriptions',{method:'POST',body:form});
    const text=(await transcription.json()).text?.trim();
    if(!text){send(res,422,'No speech detected');return;}
    const ai=await fetch(SUMANEE_URL,{
      method:'POST',headers:{'content-type':'application/json'},
      body:JSON.stringify({chatInput:text,sessionId:session}),
      signal:AbortSignal.timeout(75000)
    });
    if(!ai.ok)throw new Error('Sumanee upstream '+ai.status);
    const answer=extractText(await ai.text()).slice(0,180);
    if(!answer || answer==='Workflow got started.'){send(res,502,'Sumanee returned no answer');return;}
    const speech=await openAi('audio/speech',{
      method:'POST',headers:{'content-type':'application/json'},
      body:JSON.stringify({model:'tts-1',voice:'nova',input:answer,response_format:'wav'})
    });
    const audio=Buffer.from(await speech.arrayBuffer());
    if(audio.length>900000){send(res,502,'Voice response too large');return;}
    res.writeHead(200,{'content-type':'audio/wav','content-length':audio.length,'cache-control':'no-store'});
    res.end(audio);
  }catch(err){
    console.error('Voice request failed:',err?.message);
    if(!res.headersSent)send(res,502,'Voice upstream failed');
    else res.end();
  }
});
server.requestTimeout=120000;
server.listen(PORT,'127.0.0.1',()=>console.log('Sumanee Voice Bridge on http://127.0.0.1:'+PORT));
