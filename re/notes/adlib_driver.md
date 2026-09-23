# Infogrames "IFGM ADLIB" resident sound driver (AVE.CC1 entry 0)

File: `re/unpacked/AVE.CC1.0.bin`, a 3881-byte .COM (org 0x100). All addresses below are
**CS offsets** (file offset + 0x100). Disassemble with
`python3 x86dis.py re/unpacked/AVE.CC1.0.bin 100 START END`.
The game talks to it only through `int 0xF0` (wrappers in game segment 0e9f, see G10.md §c).
The same API is implemented by AVE.CC1 entry 1 ("IFGM BUZER PC", PC speaker) which plays the
`MUB.PAK` songs; the port only needs the AdLib one.

This document is meant to be enough to re-implement the driver in C on top of a software
OPL2 (ymfm): reproduce the **same sequence of OPL register writes per tick** and the sound is
identical. Tables that must be copied verbatim are given in full (or by address in the .bin).

---------------------------------------------------------------------------------------------
## 1. Memory map

| CS addr | size | meaning |
|---|---|---|
| 0x100 | 3 | `jmp 0xfa4` (transient installer) |
| 0x103 | 11 × 0x1f | **SFX voices** `sfx[0..10]` (voice struct, §2) |
| 0x258 | 11 × 0x1f | **music voices** `mus[0..10]` |
| 0x3ad | 11 × 10 | **hardware channel shadow** `hw[0..10]` (§2) |
| 0x41b | word | OPL base port (0x388). Status/address = base, data = base+1 |
| 0x41d | 11 × 2 | operator table, **melodic mode**: (modulator op, carrier op) per channel |
| 0x435 | 11 × 2 | operator table, **rhythm mode** |
| 0x44b | 12 words | per semitone: pointer to its 25-entry F-number row (0x463 + 0x32*n) |
| 0x463 | 12 × 25 words | F-number table (§5) – ends at 0x6bb |
| 0x6bb | 10 words | sequencer opcode handlers (opcode&0x7f = 0..9) |
| 0x6cf | 22 bytes | operator number → channel for reg 0xC0 (0xff = carrier/no write) |
| 0x6e5 | 5 bytes | rhythm key bits for channels 6..10: 0x10 BD,0x08 SD,0x04 TT,0x02 CY,0x01 HH |
| 0x6ea | far ptr | user callback for opcode 6 (null by default, fn 0x12 returns its address) |
| 0x6ee | byte | music paused flag (fn 0 does nothing while ≠0) |
| 0x6ef | 3 bytes | 0x0b, 0x07ff (unused constants: voice count / all-voice mask) |
| 0x6f2 | far ptr | = cs:0x258 (exported music-voice table, unused by game) |
| 0x6f6 | far ptr | previous int F0 vector (for uninstall) |
| 0x6fa | far ptr | music instrument bank (song + word song[0x34]) |
| 0x6fe | far ptr | SFX instrument bank (bank + word bank[0x0a]) |
| 0x702 | far ptr | SFX bank base (fn 8) |
| 0x706 | far ptr | = cs:0x3ad (exported, unused) |
| 0x70a | far ptr | "current instrument bank" (temp used during tick) |
| 0x70e | word | near ptr to the active operator table (0x41d or 0x435); 0 until fn 6/0x1a |
| 0x710 | byte | shadow of OPL reg 0xBD (initial 0x20) |
| 0x711 | byte | **melodic flag**: 0 = rhythm mode handling for channels 6..10, ≠0 = all melodic |
| 0x712.. | code | resident code up to 0xf48; 0xf49..0x1028 transient (detection + install) |

### Operator tables (verbatim)
```
melodic 0x41d: ch0 00,03  ch1 01,04  ch2 02,05  ch3 08,0b  ch4 09,0c  ch5 0a,0d
               ch6 10,13  ch7 11,14  ch8 12,15  ch9 ff,ff  ch10 ff,ff
rhythm  0x435: ch0..ch5 as above, ch6 10,13 (bass drum, 2 ops)
               ch7 14,ff (snare = carrier of ch7)   ch8 12,ff (tom = modulator of ch8)
               ch9 15,ff (cymbal = carrier of ch8)  ch10 11,ff (hi-hat = modulator of ch7)
op->ch 0x6cf:  00 01 02 ff ff ff ff ff 03 04 05 ff ff ff ff ff 06 07 08 ff ff ff
```
A pair value 0xffff means "channel absent" (voice ignored by the hardware update).

---------------------------------------------------------------------------------------------
## 2. Structures

### Voice (0x1f bytes; `bx` points to it)
| off | size | field |
|---|---|---|
| 0x00 | w | channel number 0..10 (static, = index) |
| 0x02 | w | near ptr to the linked voice of the other kind (sfx[i] ↔ mus[i]) |
| 0x04 | w | flags (below) |
| 0x06 | far | track start (0:0 = no track) |
| 0x0a | far | current read pointer |
| 0x0e | w | tick countdown to the next event batch |
| 0x10 | w | ticks per step (reload of 0x0e) |
| 0x12 | b | instrument number |
| 0x13 | w | tempo offset added to every opcode-1 duration |
| 0x15 | w | note word: low byte = note, bit 15 set = "already sent to hardware" |
| 0x17 | b | pitch bend (signed, F-number table steps; 25 steps = 1 semitone) |
| 0x18 | w | number of notes played since (re)start (opcode 2 count) |
| 0x1a | b | target volume (0..127) |
| 0x1b | b | fade countdown |
| 0x1c | b | fade speed (reload of 0x1b) |
| 0x1d | b | current volume (0..127) |
| 0x1e | b | attenuation (opcode 4) subtracted from volume for the output level |

Initial data (from the .bin): flags = 0x8040 (sfx) / 0x0040 (music), vol/target 0x7f, fade 1/1,
everything else 0. `link`: sfx[i].0x02 = &mus[i], mus[i].0x02 = &sfx[i].

Flags (0x04):
* 0x0002 restart pending: next tick rewinds to the track start and executes events at once
* 0x0004 (music voice) an SFX voice currently owns this channel
* 0x0020 loop at end of track
* 0x0040 stopped/silent
* 0x8000 SFX voice (constant; used to detect channel owner change)

### Hardware shadow `hw[ch]` (10 bytes) – last values written for this channel
| off | init | field |
|---|---|---|
| 0 | 0xffff | last note word written (with 0x8000) |
| 2 | 0x0040 | last "stopped" state (flags & 0x40) |
| 4 | 0xff | last instrument |
| 5 | 0xff | last output volume |
| 6 | 0xff | (unused) |
| 7 | 0x9c | last bend |
| 8 | 0xffff | last owner (flags & 0x8000); bit 0 set = unknown |

`hw_reset(ch)` (0x712): writes exactly these init values (not [6]).

---------------------------------------------------------------------------------------------
## 3. Data formats

### Song file ("ADLM", MUS.PAK entries *.ADD)
| off | meaning |
|---|---|
| 0x00 | "ADLM" |
| 0x08 + 4*i | dword offset (from file start) of the track for voice i (i=0..10); 0 = none. Only the low word is used as offset. |
| 0x34 | word: offset of the instrument bank (always 0x3e) |
| 0x38 | word: offset of an embedded SFX bank (0 in all game files; only used by the unused wrapper 0e9f:0144) |
| 0x3c | byte: initial value for OPL reg 0xBD (AM/VIB depth); 0 in all files |
| 0x3d | byte: melodic flag (≠0 → 9 melodic channels; 0 → rhythm mode, channels 6..10 are drums). Only AETERNAM.ADD (MUS.PAK #0) has 1. |

### SFX bank ("ADLN", MUS.PAK entry 1 "AVE.EFF")
| off | meaning |
|---|---|
| 0x00 | "ADLN" |
| 0x0a | word: offset of the instrument bank (64 instruments in AVE.EFF) |
| 0x0e | word: offset of the **effect table** (used by the game wrapper, not by the driver) |
| 0x12 + 4*n | dword offset of SFX track n (low word used), 164 tracks |

Effect table (game side, 0e9f:006f): `table = bank + w[0x0e]`; entry n at `table + w[table + 2n]`:
`word voice_mask` (bits 0..10), then one byte per set bit (ascending voice number) = SFX track
number to start on that voice. 57 effects in AVE.EFF.

### Instrument (13 bytes, bank + 13*n)
```
[0]  unused (always 0x41 'A')
modulator: [1] KSL<<6 | L (L = loudness 0..63, i.e. 63-TL)  [2] reg 0x20 (AM/VIB/EG/KSR/MULT)
           [3] reg 0xC0 (FB/CONN)  [4] reg 0xE0 (wave)  [5] reg 0x60 (AR/DR)  [6] reg 0x80 (SL/RR)
carrier:   [7] KSL|L  [8] 0x20  [9] (0xC0, never written for a carrier)  [10] 0xE0 [11] 0x60 [12] 0x80
```
A single-operator rhythm voice (SD/TT/CY/HH) uses bytes [1..6] for its operator.

### Track bytecode
Sequence of 2-byte events `op, param` (opcode 1 has one more byte). **Bit 7 of `op` ends the
batch for this tick** (the tick's event loop continues while bit 7 is clear); the handler index is
`op & 0x7f` (computed as `(op*2)&0xff`, handler table at 0x6bb):

| op&0x7f | handler | effect |
|---|---|---|
| 0 | 0xa08 | end of track: flags \|= 2; if !(flags&0x20): flags \|= 0x40 and, if this is an SFX voice, `link.flags &= ~4` (give the channel back to the music) |
| 1 | 0xa32 | `dur = param | next_byte<<8` (3-byte event); `step = countdown = dur + tempo_offset(0x13)`; saves the read pointer |
| 2 | 0xa2b | note: `notes_played++`, `note word = param` (high byte 0 → clears the 0x8000 "sent" bit) |
| 3 | 0xa4a | instrument = param |
| 4 | 0xa46 | attenuation (0x1e) = param |
| 5 | 0xa27 | bend (0x17) = param (signed) |
| 6 | 0x9f7 | far call `[0x6ea](voice_channel, param)` (cdecl, 2 words). Callback is NULL → never used; no game data contains opcode 6 |
| 7,8,9 | 0xa26 | no-op |

Note byte: bits 0-3 semitone (0 = C … 11 = B), bits 4-6 octave (OPL block), **bit 7 = key off**
(frequency still written, key-on bit cleared).
Opcodes found in all game files: 02 03 04 05 80 81 82 83 84 85 (e.g. `81 10 00` = "16 ticks per
step, end batch"; `82 1b` = "play note, end batch"; `80` = end of track).

---------------------------------------------------------------------------------------------
## 4. Entry point / install

`int 0xF0` handler at 0xf1f: `jmp short 0xf2c` followed by the signature
`"IFGM ADLIB",0` at 0xf21 (the game checks the words "IF","GM" at vector+2 and vector+4).
Handler: push ds,es,si,di,bx,cx,bp; cld; DS = CS; `call [0xf03 + AH]` (AH must be even);
pop; iret. **AX, DX are the return values; ES is not preserved by fn 6/8/0x18 (set to DX).**

Installer (0xfa4): shrink own block, detect the chip (0xf68), refuse if already installed
(signature present at the current int F0 vector → exit code 0xff), hook int F0, set 0x706/0x6f2,
TSR keeping 0xf50 bytes (`int 21h/31h`).
Chip detection 0xf68 (classic AdLib timer test): reg 1=0; reg4=0x60; reg4=0x80; status&0xe0 must
be 0; reg2=0xff; reg4=0x21; poll status until &0xe0 == 0xc0 (≤64 polls, helper 0xf49); reg4=0x60,
reg4=0x80. Port: not needed.

Register write 0x739 (`al`=reg, `ah`=value): out base,reg; 6 status reads; out base+1,val;
35 status reads. Port: `opl_write(reg, val)`.

---------------------------------------------------------------------------------------------
## 5. Function table (int F0, AH = function)

| AH | addr | name | inputs | outputs / effect | used by game |
|---|---|---|---|---|---|
| 0x00 | 0xa4e | tick | – | one sequencer tick (§6) | yes, every timer tick (60 Hz) |
| 0x02 | 0xe7e | reset all | – | for i=0..10: mus[i].flags\|=0x40, sfx[i].flags\|=0x40, hw_reset(i); then fn 4 | yes (init) |
| 0x04 | 0x8fe | chip reset | – | see below | yes (after load song, at exit) |
| 0x06 | 0xd9d | load song | DX:SI = song | see below | yes |
| 0x08 | 0xe1d | load SFX bank | DX:SI = bank | [0x702]=bank; [0x6fe]=bank+w[bank+0x0a]; all sfx voices flags\|=0x40 | yes |
| 0x0a | 0xbca | music control | SI=voice mask (0→0x7ff), DX=command bits, CX=param | AX=result, DX=sign(AX) | yes |
| 0x0c | 0xcc6 | SFX control | same | AX=result | yes |
| 0x0e | 0xe56 | uninstall | – | restore int F0 vector, free env + own block | no |
| 0x10 | 0xe4f | identify | – | AX=0x414d, DX=1 | yes (result ignored) |
| 0x12 | 0xbc4 | get callback slot | – | DX:AX = cs:0x6ea | no |
| 0x14 | 0xbbe | set port | CX = base port | [0x41b]=CX | no |
| 0x16 | 0xea1 | direct note | CH=channel, CL=note, DX=bend/flags word (goes to BP) | `key_note(ch, note, bp=DX, flags=0)` | no |
| 0x18 | 0xecd | direct instrument | DX:SI = instrument, CH=channel, CL=volume, AL bit0=skip op programming | programs ops of channel CH from [SI+1..]; then level of the modulator op with volume CL, but reading byte [SI+2] (SI was already +1: off-by-one) | no |
| 0x1a | 0xea9 | set mode | CL = melodic flag | [0x711]=CL; [0x710] = CL?0:0x20; [0x70e] = CL?0x41d:0x435 (BD not written now) | no |

### fn 4 – chip reset (0x8fe)
```
melodic(0x711) = 0;                      /* !!! see quirk Q1 */
opl(0xBD, bd_shadow); opl(4,0x60); opl(4,0x80); opl(8,0); opl(1,0x20);
for ch 0..8: opl(0xA0+ch,0); opl(0xB0+ch,0);
for ch 0..10: hw_reset(ch);
for ch 6,7,8: opl(0xA0+ch,0); opl(0xB0+ch,0);
set_freq(ch=8, note=0x00, bend=0, flags=0x40);   /* 0x843: tom pitch C-0, key off */
set_freq(ch=7, note=0x07, bend=0, flags=0x40);   /* snare/hihat pitch G-0 */
```

### fn 6 – load song (0xd9d), ES:SI = song
```
optable = 0x41d; al = song[0x3c]; melodic = song[0x3d];
if (!melodic) { al |= 0x20; optable = 0x435; }
bd_shadow = al;                                   /* not written to the chip here */
for i 0..10: off = dword song[8+4i];
             mus[i].start = off ? song + (uint16)off : NULL;  mus[i].flags |= 0x40;
music_bank = song + w[song+0x34];
```
(The game always calls fn 4 right after fn 6: that writes BD and resets `melodic` to 0.)

### fn 0x0a – music control (0xbca) / fn 0x0c – SFX control (0xcc6)
`result = 0xffff`. For each voice i (0..10) with bit i of the mask (mask 0 = all):
music: skip voices whose track start is NULL. Then, in this order:

| DX bit | music (fn 0xa) | SFX (fn 0xc) |
|---|---|---|
| 0x0100 | CX≠0: paused=0; CX=0: paused=1 and fn 4 (silence). *Only inside this bit:* if DX&0x400: tempo_offset = CX | – |
| 0x0400 | (only with 0x100, see above) | tempo_offset = CX |
| 0x0040 | stop: flags \|= 0x40 | stop: flags \|= 0x40 (link's 0x04 is NOT cleared, quirk Q3) |
| 0x0080 | start: flags=0x40; vol=target=0x7f; atten=0; hw_reset(ch); flags=0x0002 (restart, no loop) | if DX&0x4000 and voice still playing → no restart. Else start SFX track CX: start = bank + w[bank+0x12+4*CX]; flags=0x8002; atten=0; link.flags \|= 4; hw_reset(ch) |
| 0x0020 | flags \|= 0x20 (loop) | same |
| 0x2000 (without 0x10) | CX &= 0x7f; vol = target = CX | same |
| 0x8000 | CX &= 0x7f; target = CX (fade toward it) | same |
| 0x1000 | fade speed: 0x1b = 0x1c = CL | – |
| 0x0010 | query: without 0x2000: if voice playing and !(result > notes_played, *signed*) → result = notes_played, i.e. result = **maximum** notes_played over the playing voices (0xffff = -1 if none plays). With 0x2000: if vol ≠ CL → result = 0 ("fade not finished") | same |
| 0x0200 | at the end (music only): return DX:AX = cs:0x6ea instead of the result | – |

Return AX = result (0xffff if nothing matched), DX = AX sign-extended (music) / unchanged (SFX).
CX is modified in place by the `&= 0x7f` and the modified value is used for the following voices.

---------------------------------------------------------------------------------------------
## 6. The tick (fn 0, 0xa4e)
```
if (paused) return;
for i = 0..10:
    bank = music_bank;               step_voice(&mus[i]);                /* 0x979 */
    v = &mus[i];
    if (mus[i].flags & 4) { bank = sfx_bank; v = &sfx[i]; step_voice(v); }
    update_hw(v, &hw[i]);                                                  /* 0xaad */
```
So a music voice keeps running (in time) while an SFX owns its channel; SFX voices are only
advanced while they own a channel.

### step_voice (0x979)
```
if (flags & 0x40) return;
if (flags & 2) { ptr = start; flags &= ~2; notes_played = 0; goto events; }  /* no fade, no countdown */
if (target != vol) {                                  /* fade */
    if (--fade_cnt < 0) {                             /* signed byte */
        fade_cnt = fade_speed;
        int8 d = (target > vol /*unsigned*/) ? +1 : -1;
        uint8 n = vol + d;  if (n & 0x80) n = 0;  if (n >= 0x7f) n = 0x7f;  vol = n;
    }
}
if (--countdown != 0) return;                          /* 16-bit */
countdown = step;
events:
do { op = *ptr++; param = *ptr++; handler[op & 0x7f](param); } while (!(op & 0x80));
```
(Opcode 1 reads its extra byte and saves `ptr` itself; the other handlers don't move `ptr`.
Opcode 0 does not stop the loop by itself: game data always uses 0x80.)

### update_hw (0xaad), `v` = voice, `h` = hw shadow of channel i
```
s = v->flags & 0x40;
if (s != h->stopped) {
    h->stopped = s;
    if (s) {  /* just stopped: key off */
        key_note(ch=v->channel, note=(v->note & 0xff) | 0x80, bend=v->bend, flags=0x40);
        hw_reset(v->channel);
        return;
    }
}
if (s) return;
o = v->flags & 0x8000;
if ((h->owner & 1) || o != h->owner) { h->owner=o; h->vol=0xff; h->inst=0xff; h->note=0xffff; h->bend=0x9c; }
ops = optable[v->channel];  if (ops == 0xffff) return;       /* cl = op1, ch = op2 */
if (v->inst != h->inst) {
    h->inst = v->inst;
    ins = bank + 13*v->inst;
    program_op(op1, ins+1);  if (op2 != 0xff) program_op(op2, ins+7);   /* 0x8b4 */
    h->vol = 0xff;
}
out = max(0, v->vol - v->atten);
if (out != h->vol) {
    h->vol = out;
    set_level(op1, ins[1], (op2 == 0xff) ? out : v->vol);   /* modulator uses the raw volume */
    set_level(op2, ins[7], out);                            /* skipped if op2 == 0xff */
}
bp = (uint8)v->bend;  nw = v->note;
if ((uint8)v->bend != h->bend) { h->bend = v->bend; if (nw == h->note) bp |= 0x8000; /* legato */ }
else if (nw == h->note) return;
nw |= 0x8000;  h->note = nw;  v->note = nw;
key_note(ch=v->channel, note=nw & 0xff, bend=bp, flags=v->flags);        /* 0x7b4 */
```
`program_op(op, p)` (0x8b4, p = operator record, uses p[1..5]):
`if (opch[op]!=0xff) opl(0xC0+opch[op], p[2]); opl(0x60+op,p[4]); opl(0x80+op,p[5]); opl(0x20+op,p[1]); opl(0xE0+op,p[3]);`
(careful: here p = ins+1, so p[1]=ins[2] … p[5]=ins[6].)

`set_level(op, b, vol)` (0x774; skip if op == 0xff):
`t = 63 - (2*(b&0x3f)*vol + 127) / 254;  opl(0x40+op, (b & 0xc0) | t);`

### key_note(ch, note, bp, flags) (0x7b4)
```
if (melodic || ch < 6) { set_freq(ch, note, bp, flags); return; }
/* rhythm channels 6..10 */
f = 0x40;                                            /* frequency writes keep key-on off */
if (ch == 6) set_freq(6, note, bp, 0x40);
else if (ch == 8 && !(note & 0x80)) {
    set_freq(8, note, bp, 0x40);
    n = (note & 0x0f) + 7; o = note & 0x70;
    if (n >= 12) { n -= 12; if (o != 0x70) o += 0x10; }
    set_freq(7, o | n, bp, 0x40);                    /* snare/hi-hat pitch = tom + a fifth */
}
bit = rhythm_bit[ch-6];                              /* 0x10,8,4,2,1 */
bd = bd_shadow & ~bit;  opl(0xBD, bd);               /* retrigger: key off */
if (!(flags & 0x40) && !(note & 0x80)) { bd |= bit; opl(0xBD, bd); }
bd_shadow = bd;
```
(Channels 7, 9, 10 never write a frequency themselves.)

### set_freq(ch, note, bp, flags) (0x843)
```
if (!(bp & 0x8000)) opl(0xB0+ch, 0);                 /* key off before a new note */
row = fnum_row[note & 0x0f];                          /* table 0x44b */
if ((bp & 0x80) && (note & 0x0f) == 0) {              /* C with downward bend → B row of octave-1 */
    row = end_of_table (0x6bb);  if (note & 0x70) note -= 0x10;
}
if (note & 0x80) flags = 0x40;
fn = row[(int8)bp];                                   /* signed index, may run into neighbour rows */
opl(0xA0+ch, fn & 0xff);
v = ((note & 0x70) >> 2) + (fn >> 8);                 /* block<<2 | fnum high bits */
if (!(flags & 0x40)) v |= 0x20;                       /* key on */
opl(0xB0+ch, v);
```
F-number table (0x463, 12 rows × 25 words; row n = semitone n, entry k = +k/25 semitone).
The rows are contiguous so a bend ≥ 25 or < 0 reads neighbouring rows (and past row 11, the
opcode table at 0x6bb… – copy the bytes 0x463..0x6ce verbatim to be exact):
```
C  343,344,345,346,346,347,348,349,350,351,351,352,353,354,355,356,356,357,358,359,360,360,361,362,363
C# 364,365,365,366,367,368,369,370,371,372,372,373,374,375,376,377,378,379,379,380,381,382,383,384,385
D  385,387,387,388,389,390,391,392,393,394,395,396,397,398,398,399,400,401,402,403,404,405,406,407,408
D# 408,410,410,411,412,413,415,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432
E  433,434,435,436,437,438,439,440,441,442,443,444,445,447,448,449,450,451,452,453,454,455,456,457,458
F  459,460,461,462,463,464,466,467,468,469,470,471,472,473,474,475,477,478,479,480,481,482,483,484,485
F# 486,488,489,490,491,492,493,495,496,497,498,499,500,502,503,504,505,506,507,509,510,511,512,513,514
G  515,517,518,519,520,522,523,524,525,527,528,529,530,532,533,534,535,537,538,539,540,541,543,544,545
G# 546,548,549,550,551,553,554,556,557,558,559,561,562,564,565,566,567,569,570,571,572,574,575,577,578
A  579,581,582,583,584,586,587,589,590,592,593,594,596,597,599,600,601,603,604,606,607,608,610,611,612
A# 614,615,617,618,619,621,622,624,625,627,628,630,631,633,634,636,637,639,640,642,643,645,646,648,649
B  650,652,653,655,657,658,660,661,663,665,666,668,669,671,672,674,675,677,679,680,682,683,685,687,688
```

---------------------------------------------------------------------------------------------
## 7. Timing

The driver has no timer of its own: the game calls fn 0 from its int 8 handler (registered
callback 0e9f:0116), i.e. at the game's PIT rate **1193182/0x4dae = 60.0006 Hz** (G10.md §b).
All durations (opcode 1) are in 1/60 s ticks; fades move 1 volume step every `fade_speed+1`
ticks (default speed 1 → every 2 ticks). Implemented in `src/sound/adlib_drv.c`, ticked by the audio thread at
`adlib_drv_hz = 1193182.0/19886` (src/sound/audio.c).

---------------------------------------------------------------------------------------------
## 8. How the game drives it (0e9f + 0791 wrappers)

* init: detect, register tick, fn 2, fn 0x10.
* song n: `sub_0791_0078(n)`: stop music (fn 0xa, 0x40) → load MUS.PAK entry n (name at
  DSPTR(0x18b0); third letter patched to 'b' → MUB.PAK when the launcher config says "no AdLib")
  into DSPTR(0x17c0) → fn 6 + fn 4 → music volume `sub_0791_0005(DS16(0xe8))` (fn 0xa 0x2000,
  or 0x40 if 0) → if DS16(0xe8)≠0: fn 0xa start (0x80) on all voices (**no loop flag**, volume back
  to 127).
* SFX bank: `sub_0791_0048(1)` loads MUS.PAK #1 (AVE.EFF) into DSPTR(0x17c4) → fn 8.
* effect n: `sub_0791_00e5(n)`: if music vol > 50: fn 0xa 0x2000 vol=50 then fn 0xa 0x8000
  target=DS16(0xe8) (duck + fade back); `sub_0e9f_006f(0,0,0x40)` = stop the voices of **effect #0** only
  (mask 0x14 = voices 2 and 4 in AVE.EFF; other SFX voices keep playing to their end);
  if DS16(0xea) > 0: start effect n (fn 0xc 0x80 per voice). `sub_0791_0132`: same with 0xa0 (loop).
* shutdown: unregister tick, fn 4.

---------------------------------------------------------------------------------------------
## 9. Quirks that change the audible result (replicate them)

* **Q1 – fn 4 clears the melodic flag.** The game always does fn 6 then fn 4, so after loading a
  melodic song (only AETERNAM.ADD, the title music, has song[0x3d]=1) `melodic` is 0 while the
  operator table (0x41d) and BD shadow (no 0x20) are the melodic ones. Result: channels 6, 7 and 8
  go through the rhythm path: ch6/ch8 get frequencies with key-on cleared and only (ineffective)
  BD bits, ch7 never gets a frequency → **tracks 6..8 of AETERNAM are silent in the original**.
  Rhythm songs are unaffected (fn 4 leaves 0x711 = 0 which is what fn 6 set).
* **Q2 – music never loops** (0x20 not set by the game); each song stops at its end marker.
* **Q3 – stopping an SFX (fn 0xc 0x40) does not return the channel to the music** (link bit 4 stays
  set, the channel stays silent) until the SFX voice reaches its end opcode on a later start, or
  the song is restarted (fn 0xa 0x80 rewrites the flags). The game stops "effect 0"'s voices
  before each new effect and, if the SFX volume is 0, starts nothing → those music channels stay
  muted until the next song.
* **Q4 – music start ignores the volume**: fn 0xa 0x80 sets vol=127 after the game set its volume.
* **Q5 –** `sub_07fe_0ff1` calls `sub_0e9f_006f(sfxvol, 0, 0x2000)`: the first argument is an
  effect index, so it sets the volume of the voices of effect #sfxvol to that effect's *track
  numbers*. Harmless nonsense but it reads the effect table with index up to ~120 (> 57 entries):
  the C wrapper must read with 16-bit offset wrap inside the bank buffer and not crash.
* Modulator level is scaled by the volume too (see update_hw), not only the carrier.
* Opcode 5 bend applies at the next note/bend change; a bend change on the same note re-writes
  the frequency without key-off (legato, bp bit 15).

---------------------------------------------------------------------------------------------
## 10. Suggested C shape
```c
void adl_reset_all(void);                       /* fn 2 */
void adl_chip_reset(void);                      /* fn 4 */
void adl_load_song(const uint8_t *song);        /* fn 6 */
void adl_load_sfx(const uint8_t *bank);         /* fn 8 */
uint16_t adl_music_ctl(uint16_t mask, uint16_t cmd, uint16_t param);  /* fn 0xa */
uint16_t adl_sfx_ctl  (uint16_t mask, uint16_t cmd, uint16_t param);  /* fn 0xc */
void adl_tick(void);                            /* fn 0, 60 Hz */
```
State is plain native C (voices keep native `const uint8_t *` pointers into the game's song/bank
buffers, which live in the arena and are never freed while in use). If `adl_tick` runs on the
audio thread, every call from game code must hold `plat_audio_lock()` (the original was protected
by being inside the int 8 handler, with interrupts effectively serialised).
