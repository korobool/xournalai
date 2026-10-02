import os, sys; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib"))
import numpy as np
from conspect import Sheet
W,H=1190,842
s=Sheet(W,H,ink="#1b1b1b",seed=41)
TE,RD,IN,OR,BL,GR,GY,DK,PU,GN,LG="#00897b","#c62828","#3949ab","#ef6c00","#1565c0","#607d8b","#546e7a","#1b1b1b","#6a1b9a","#2e7d32","#9e9e9e"

def A(a,b,col="#37474f",dash=False,head=3.4,w=0.9):
    a=np.array(a,float); b=np.array(b,float)
    if dash:
        L=np.linalg.norm(b-a); d=(b-a)/L; t=0
        while t<L-4:
            s.stroke([a+d*t,a+d*min(t+3,L-4)],col,w,wobble=False); t+=5.5
        s.arrow(a+d*(L-4),b,color=col,head=head,width=w)
    else: s.arrow(a,b,color=col,head=head,width=w)
def P(pts,col,w=0.9,dash=False):
    for a,b in zip(pts[:-2],pts[1:-1]):
        if dash: A(a,b,col,True,head=0.01,w=w)
        else: s.stroke([a,b],col,w)
    A(pts[-2],pts[-1],col,dash,w=w)
def _boost(sz): return min(sz*1.12,2.8) if sz<2.8 else sz
def T(x,y,t,c=DK,sz=2.7,w=0.62): sz=_boost(sz); s.text(x,y,t,size=sz,color=c,width=w)
def C(cx,y,t,c=DK,sz=2.7,w=0.62):
    sz=_boost(sz); s.text(cx-s.measure(t,sz)/2,y,t,size=sz,color=c,width=w)
FIT=[]
def box(x,y,w,h,col,title=None,lines=(),tsz=3.2,sz=2.45,ls=5.6,center=False,amp=0.0,lw=1.1,tcol=None):
    s.cloud(x,y,w,h,color=col,width=lw,amp=amp,radius=4)
    L=[(l[0] if isinstance(l,tuple) else l) for l in lines]
    ratio=ls/sz
    # auto-fit: largest body size (<= 3.6) that fits the box width and height
    z=sz
    for cand in [4.2,4.1,4.0,3.9,3.8,3.7,3.6,3.5,3.4,3.3,3.2,3.1,3.0,2.9,2.8,2.7,2.6,2.5,2.4]:
        if cand<sz: break
        tz=max(tsz,cand+0.25) if title else 0
        lsc=cand*ratio
        wid=max([s.measure(l.lstrip(),cand)+(len(l)-len(l.lstrip()))*s.font.space*cand for l in L]+[s.measure(title,tz) if title else 0])
        hei=7.5+(tz*1.25+cand*1.45 if title else 0)+max(len(L)-1,0)*lsc+cand*0.8
        if wid<=w-8 and hei<=h-3: z=cand; break
    if z!=sz: FIT.append((title,sz,z))
    tsz=max(tsz,z+0.25) if title else tsz; ls=z*ratio; sz=z
    yy=y+7.5
    if title:
        (s.text(x+w/2-s.measure(title,tsz)/2,yy,title,size=tsz,color=tcol or col,width=0.85) if center else s.text(x+4,yy,title,size=tsz,color=tcol or col,width=0.85)); yy+=tsz*1.25+sz*1.45
    for l in lines:
        c=DK
        if isinstance(l,tuple): l,c=l[0],l[1]
        (s.text(x+w/2-s.measure(l,sz)/2,yy,l,size=sz,color=c,width=0.7) if center else s.text(x+4,yy,l,size=sz,color=c,width=0.7)); yy+=ls
    return yy
def badge(x,y,n,col):
    s.cloud(x-4,y-4,8,8,color=col,width=0.9,amp=0,radius=4); C(x,y+1.5,n,col,2.9,0.85)


s.heading(22,20,"Quote data: insurers -> canonical JSON -> AI thinking core & broker app",size=6.0,color=BL,width=1.0)
T(24,31,"READ RIGHT -> LEFT.   thick coloured = response data (the price and everything about it);   thin grey dashed = the request that triggered it (left -> right)",GY,2.8)
X={"ins":(1105,1176),"drv":(912,1084),"map":(730,892),"bld":(544,710),"str":(366,522),"sto":(194,346),"hub":(22,174)}
hdr=[("ins","insurers",TE,"1"),("drv","scraper logic (drivers)",PU,"2"),("map","mappers",GN,"3"),("bld","canonical JSON builder",TE,"4"),
     ("str","stream + wrap",OR,"5"),("sto","store + service API",IN,"6"),("hub","broker-tools-hub (MCP)",BL,"7")]
for k,t_,c,n in hdr:
    cx=sum(X[k])/2; C(cx,50,t_,c,3.4,0.9); badge(cx-s.measure(t_,3.4)/2-7,48.8,n,c)
A((200,61),(1100,61),LG,dash=True,w=0.7)
T(210,58.5,"request: POST /quotes/held  {job_id, fields = FLAT canonical bag, addons, carriers_config (pushed)}  ->  resolvers  ->  rule table  ->  carrier model  ->  wire JSON  ->  insurer",LG,2.5)
rows=[("Simpego","simpego/drive.py :: drive",["data.productPrice.price {chf, rappen}","-> _amount() -> Decimal","+ quoteId (RAISES if absent)"],
        "mapping/carriers/simpego.py",["Rule(field='driver_under_25',","  model_field='driver_too_young')","-> SimpegoModel + wire() paths"]),
      ("Helvetia","helvetia/drive.py :: drive",["calculatedProposal.salesProduct","  .premium.grossPremium","-> price (CHF / year)"],
        "mapping/carriers/helvetia.py",["Rule table -> HelvetiaQuote","ledger: status per field","disclosures (mapping-time)"]),
      ("Generali","generali/drive.py :: drive",["PR_Jahrespraemie; half-yearly:","  PR_Halbjahrespraemie x 2","  2x831.40 = 1'662.80 vs 1'633.40"],
        "mapping/carriers/generali.py",["Rule table -> GeneraliQuote","ledger: status per field","disclosures (mapping-time)"])]
yr=[104,170,236]
for (ins,drv,dl,mp_,ml),y in zip(rows,yr):
    s.cloud(1105,y-12,71,24,color=TE,width=1.0,amp=1.0,period=7,radius=8); C(1140.5,y+1.3,ins,DK,3.3,0.78)
    A((1104,y),(1085,y),TE,w=1.6)
    box(912,y-28,172,56,PU,drv,dl,tsz=3.2,sz=2.8,ls=6.8)
    box(730,y-28,162,56,GN,mp_,ml,tsz=3.0,sz=2.8,ls=6.8)
    A((911,y+6),(893,y+6),PU,w=1.6); T(893,y+15,"QuoteResult",PU,2.2)
    A((729,y),(711,y),GN,w=1.6)
s.cloud(1105,268,71,20,color=TE,width=0.9,amp=0.9,period=7,radius=7); C(1140.5,277,"+8 more on v2 (11 tables)",GY,2.3); C(1140.5,283,"baloise v1; 12/13 on",GY,2.3)
C(998,276,"resolve -> resolved -> DRIVE -> offer_drive",PU,2.6); C(998,283,"response side = IMPERATIVE, per carrier",PU,2.6)
C(811,276,"request side = DECLARATIVE",GN,2.6); C(811,283,"no response rule table",GN,2.6)
C(848,300,"QuoteResult (scrapers/pipeline/contracts.py): price, currency='CHF', period='year', declined=Skip|None, echo_vehicle, session{model, quote_id}, extras, disclosures[...]",PU,2.5)
C(848,308,"declined = Skip('declined') is a RESULT, not an exception; only transport / contract failures raise",PU,2.5)
box(544,70,166,222,TE,"canonical JSON builder",[
 ("quote_engine._attempt + service._quote_dict",GY),"",
 ("NORMALIZATION (all of it happens here):",TE),
 "- CHF / year DECLARED by the driver; no conversion step",
 "- applied = projection of the REAL mapped values:",
 "   deductibles snapped to the carrier's ladder,",
 "   add-ons as booleans, _disclosed rows",
 "- source: real | estimate | mock | unavailable | error",
 "- binding: true only for real",
 "- disclosures: drive-time + mapping-time, one shape",
 "- multiplier: scales MOCK prices only",
 "- key_used: outcome key_bound (tg | eurotax | vector)",
 "- consumed_fields: field names only",
 "- + mapper_result ledger (null off the v2 path)"],tsz=3.4,sz=2.75,ls=7.4)
for yy in np.arange(66,318,7): s.stroke([(533,yy),(533,yy+4)],RD,0.8,wobble=False)
box(366,72,156,50,OR,"NDJSON stream",["one {kind, payload} per line","progress | quote | error | done","dropped conn = cancel; run cap 25 s,","20 s per carrier; api reader open 180 s"],tsz=3.2,sz=2.7,ls=6.6)
A((543,97),(523,97),TE,w=1.6); T(526,93,"frame",TE,2.3)
box(366,138,156,78,OR,"broker-api  /  broker-portal",["ws.go  /  quotes.go: wrap, change nothing","WS {type: quote, quote: <frame>}","portal pub('quote', {quote: p})","  + rawQuotes (all full frames)","+ UpdateQuoteRunCarrier -> DB"],tsz=3.2,sz=2.7,ls=6.6)
A((444,123),(444,137),OR,w=1.6)
T(462,236,"normalization",RD,2.7); T(462,243,"ends here;",RD,2.7); T(462,250,"downstream:",RD,2.6); T(462,257,"wrap + persist",RD,2.6); T(462,264,"VERBATIM",RD,2.6)
box(194,72,152,70,IN,"Postgres quote_runs",["results jsonb: {carrier_id: frame}","per carrier, last write wins:","  jsonb_set(results, {id}, frame)","input = {fields, addons, carriers?}"],tsz=3.2,sz=2.7,ls=6.6)
A((365,170),(347,120),OR,w=1.6)
box(194,160,152,64,IN,"service API (broker-api)",["GET /api/service/chat/prices","  ?chat_id=&user_id=  (X-API-Key)","LATEST run of the chat","DB-only: costs the scraper nothing"],tsz=3.2,sz=2.7,ls=6.6)
A((270,143),(270,159),IN,w=1.6)
box(22,72,152,178,BL,"broker-tools-hub",[("MCP server, Go: tools.go",GY),"",
  ("quote_run",BL),"  side effect: POST .../quote-run","  {user_id*, fields*, addons,","   carriers, chat_id} -> live run","",
  ("quote_status",BL),"  read-only: {chat_id*, user_id}","  -> GET .../chat/prices","  -> body as tool TEXT, unmodified","",
  ("backend = broker-api OR portal",BL),("  (backend_url, ADR-0013)",BL)],tsz=3.4,sz=2.7,ls=6.9)
A((193,192),(175,192),IN,w=1.6); T(177,188,"JSON",IN,2.3)

# B1 annotated frame
code=[
'{',
' "carrier_id": "simpego", "carrier_name": "Simpego",',
' "source": "real", "binding": true,',
' "key_used": "vector", "attempts": 1,',
' "elapsed_ms": 2341, "tokens": 0,',
' "price": 1153.6, "currency": "CHF", "period": "year",',
' "applied": {"coverage": "Vollkasko",',
'    "deductible_kasko": 1000, "addon_glass": true,',
'    "_disclosed": [{field, label, original, adjusted, reason}]},',
' "adjustments": [{"field": "gender", "label": "...",',
'    "original": "maennlich", "adjusted": "male",',
'    "audience": "customer", "reason": "..."}],',
' "error": null, "note": null,',
' "refusal": null | {"code": "missing",',
'    "errors": [{group, fields: ["residence_permit"], detail}]},',
' "consumed_fields": [...],',
' "mapper_result": {"ok": true, "carriers": {"simpego": {',
'    "model_type": "SimpegoModel", "model": {...filed body...},',
'    "fields": [{"field": "addon_glass", "status": "mapped",',
'      "records": [{"path": "tariffOptions[...].attributes[glass]",',
'        "value": "Plus", "action": "coarsened"}]}]}}}',
'}']
X1,Y1,LS,SZ=790,356,8.6,2.95
s.cloud(X1-8,Y1-12,W-17-(X1-8),len(code)*LS+18,color=TE,width=1.2,amp=0.0,radius=6)
badge(X1-2,Y1-4,"4",TE); T(X1+6,Y1-2.5,"quote frame = canonical JSON (one per carrier)   then downstream shapes <- right to left",TE,3.2,0.9)
s.code(X1,Y1+8,"\n".join(code),size=SZ,color=DK,line=LS)
cw=max(s.measure(l.lstrip(),SZ)+ (len(l)-len(l.lstrip()))*s.font.space*SZ for l in code)
bx=X1+cw+5
secs=[(1,1,"identity",["which carrier"],DK),
      (2,4,"provenance",["real|estimate|mock|unavailable|error; binding = true only for real"],GY),
      (5,5,"price (normalized)",["CHF / year declared by the driver"],TE),
      (6,8,"applied",["REAL mapped values (compare table); legacy carriers: request echo"],GN),
      (9,11,"adjustments",["disclosures from drive + mapping"],OR),
      (12,14,"error / refusal",["declined / missing inputs: a result, not a crash"],RD),
      (15,15,"consumed_fields",["field NAMES the carrier module maps (CONSUMED_FIELDS), no values"],GY),
      (16,20,"mapper_result",["LEDGER: where each field landed (records[].path); null if carrier not on v2 path"],GN)]
for a,b,t,ex,c in secs:
    y0=Y1+8+(a)*LS-6.5; y1=Y1+8+(b)*LS+1.5
    s.brace(bx,y0,y1,side="right",depth=4,color=c,width=0.8)
    ym=(y0+y1)/2+1.2; T(bx+7,ym,t,c,2.9,0.85)
    x2=bx+9+s.measure(t,2.9)
    T(x2,ym," - "+" ".join(ex),GY,2.35)


# ================= MIDDLE BAND: consumers, each directly above the JSON it receives
C(300,340,"AI thinking core  /  broker application",RD,3.4,0.9); badge(300-s.measure("AI thinking core  /  broker application",3.4)/2-7,338.8,"8",RD)
# AI core under the tools hub
s.cloud(22,350,266,236,color=RD,width=1.3,amp=0.0,radius=6)
T(28,361,"AI thinking core (agentic engine)",RD,3.4,0.9)
box(30,370,250,40,RD,"broker-llm-agent (orchestrator)",["quote_status is in its backend-routed tool set","(backend_ctx.py)"],tsz=3.0,sz=3.1,ls=7.6)
box(30,416,250,40,RD,"agent-car / agent-house",["shared vendored connector","connectors/broker_api.py :: quote_status"],tsz=3.0,sz=3.1,ls=7.6)
T(32,470,"The LLM reads the tool-result JSON:",DK,3.1); T(32,478.5,"priced / unpriced carriers + the FULL frames",DK,3.1)
T(32,487,"-> compares, explains, answers in chat",DK,3.1)
T(32,497,"best offer: chosen at answer / render time,",GY,2.9); T(32,505,"not stored  [inferred]",GY,2.9)
box(30,514,250,26,LG,None,["skill / rag: never carry the quote JSON (prompt text only)"],sz=2.5,ls=6,tcol=LG,lw=0.8)
T(32,556,"reads prices only via MCP quote_status -> backend",BL,2.9); T(32,563,"GET /api/service/chat/prices (DB-only, never the scraper)",BL,2.9)
# broker application under stream + wrap
s.cloud(300,350,262,236,color=OR,width=1.3,amp=0.0,radius=6)
T(306,361,"broker application",OR,3.4,0.9)
box(308,370,246,80,IN,"Flutter client",["live frames via broker-api WS:","{type: quote, quote: <frame>}","{type: progress, done, total}","progress fields SPREAD to the top level","(ws_messages.dart reads them there)"],tsz=3.0,sz=3.1,ls=7.6)
box(308,458,246,82,OR,"portal UI",["pub('quote', {quote: p}) per carrier","every full frame appended to rawQuotes","end: SetPhase(chat, user, 'quoted', 'auto',","  rawQuotes) -> _quotes -> compare columns","(without full frames: 'No details returned')"],tsz=3.0,sz=3.1,ls=7.6)
T(308,556,"receives frames LIVE, without MCP:",OR,2.9); T(308,563,"the frame is byte-identical on both edges",OR,2.9)
# arrows: straight down
A((98,251),(98,349),BL,w=1.6); T(102,300,"MCP: quote_status",BL,2.7)
A((444,217),(444,349),OR,w=1.6); T(410,296,"live",OR,2.9); T(404,303,"frames",OR,2.9)
P([(320,225),(320,316),(640,316),(640,349)],IN,w=1.2); T(644,340,"persisted row",IN,2.5)
P([(626,293),(626,330),(900,330),(900,343)],TE,w=1.2)
A((155,587),(155,626),RD,w=1.4)
A((431,587),(431,626),OR,w=1.4)
A((781,450),(773,450),TE,w=1.6)
s.stroke([(22,322),(1175,322)],LG,0.6,wobble=False)


# ================= B-row (y 627): directly under their consumers; B2 next to B1
box(22,627,266,166,BL,"what the LLM sees (quote_status result)",[
 ("GET /api/service/chat/prices -> tool TEXT",GY),
 '{ "found": true,',
 '  "run_id": "...", "job_id": "...",',
 '  "status": "done", "mode": "quote",',
 '  "carriers_priced": 2,  "input": {...},',
 ('  "carriers": {"simpego": {...whole frame...},',TE),
 ('               "helvetia": {...}},',TE),
 '  "priced": ["simpego", "helvetia"],',
 ('  "unpriced": [{"carrier_id": "vaudoise",',RD),
 ('                "reason": "..."}] }',RD),
 ("read-only, DB only: polling costs the scraper nothing",GY)],tsz=3.2,sz=3.5,ls=12.0)
box(300,627,262,166,OR,"what the broker app receives",[
 ("Flutter (broker-api WS, ws.go)",IN),
 '{"type": "quote", "quote": <frame>}',
 '{"type": "progress", "carrier_id": "...", "status": ...}',
 '{"type": "progress", "done": 1, "total": 12}  // 2 frames',
 ("portal (quotes.go)",OR),
 'event quote:    {"quote": <frame>}',
 'event progress: {"done": 1, "total": 12}',
 'end: SetPhase(chat, user, "quoted", "auto", rawQuotes)',
 '     -> _quotes -> agent compare columns',
 ("only the wrapper differs",DK)],tsz=3.2,sz=3.5,ls=12.0)
box(572,350,200,236,IN,"quote_runs row (Postgres)",[
 ("00017_quote_runs.sql",GY),
 "{ id, job_id, user_id, chat_id,",
 "  cohort: 'tmp', mode: quote|offer,",
 "  status: running|done|timeout|failed,",
 "  input: {fields, addons, carriers?},",
 ("  results: {'simpego': <frame>,",TE),
 ("            'helvetia': <frame>, ...},",TE),
 "  carriers_priced, created/updated/",
 "  finished_at }",
 ("merge: jsonb_set per carrier",IN),
 ("close: FinalizeQuoteRun(job, status, n)",IN)],tsz=3.2,sz=3.2,ls=12.0)


# ================= key facts: right column under the canonical JSON, wrapped for reading
facts=[
 ("Direction","the mappers turn the canonical bag INTO the carrier's JSON (request). The price comes back through each carrier's own drive.py (response); there is no response rule table."),
 ("One canonical JSON","the per-carrier QUOTE FRAME, built only in the scraper (quote_engine._attempt + service._quote_dict). All normalization happens there."),
 ("Light normalization","CHF/year is declared by each driver (Generali converts half-yearly x2 itself); 'applied' holds the real mapped values; the multiplier scales MOCK prices only."),
 ("Verbatim downstream","broker-api and the portal wrap the frame, persist it into quote_runs.results[carrier_id] and stream it to their clients, changing nothing."),
 ("AI core reads the DB","MCP quote_status -> backend GET /api/service/chat/prices -> DB-only read; never the scraper. On portal turns the backend is the portal."),
 ("Declines are results","Skip / refusal shows up in 'unpriced' with a reason; only transport or contract failures raise."),
 ("Full frames matter","the compare table is built from the FULL frames (rawQuotes -> _quotes); without them every column reads 'No details returned'.")]
KX,KY,KW=572,604,603
def wrap(txt,sz,width):
    out,cur=[], ""
    for w_ in txt.split():
        cand=(cur+" "+w_).strip()
        if s.measure(cand,sz)>width and cur: out.append(cur); cur=w_
        else: cur=cand
    out.append(cur); return out
yy=KY+14
blocks=[]
for i,(h,body) in enumerate(facts):
    lines=wrap(body,3.8,KW-30)
    blocks.append((i,h,lines,yy)); yy+=10+len(lines)*9.2+4.8
s.cloud(KX,KY,KW,yy-KY+2,color=RD,width=1.1,amp=0.0,radius=6)
T(KX+7,KY+10,"Key facts (from the code)",RD,3.8,0.95)
for i,h,lines,y0 in blocks:
    badge(KX+11,y0+5.5,str(i+1),RD)
    T(KX+20,y0+8,h,RD,3.8,0.9)
    for k,l in enumerate(lines): T(KX+20,y0+17.8+k*9.2,l,DK,3.8,0.72)
print("facts bottom",yy)
s.text(24,812,"sources: broker-scraper app/scrapers/carriers/<id>/drive.py, app/scrapers/pipeline/contracts.py, app/services/quote_engine.py, app/services/carriers.py;",size=3.6,color=GY,width=1.0)
s.text(24,822,"broker-api internal/store/quote_runs.go, internal/httpapi/service_quote_results.go, internal/ws/ws.go;  broker-tools-hub internal/tools/tools.go;  broker-portal internal/httpapi/quotes.go",size=3.6,color=GY,width=1.0)

for _i,(_p,_c,_w,_f) in enumerate(s.paths): s.paths[_i]=(_p,_c,min(_w*1.4,2.6),_f)
print("\n".join(map(str,FIT)))
s.save("/tmp/claude-1000/-home-olek/085dba93-7e8f-4379-915c-c2a70c5af9f3/scratchpad/flow_v4")
allp=s.paths; n=len(allp); k=3
for i in range(k):
    s.paths=allp[i*n//k:(i+1)*n//k]
    open(f"flow_v4_part{i}.svg","w").write(s.svg())
