## Accrip

Original thread: https://hydrogenaudio.org/index.php/topic,61574.0.html

Omy's port/rewrite to C of the ARFlac.pl script which was in turn based off ARCue.pl.
sarcasticadmin took the code and patches from the above thread and created this repo.
Additionally there have been small tweaks to make this compile on other Linux systems
like NixOS.

### Reqs

You'll need [flac](https://xiph.org/flac/) and [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
before attempting to build.

### Run

```
$ accuraterip-crcgen  ./Operation_Ivy/Operation_Ivy/

[Disc ID: 0031e747-03c145ac-870be71b] Found 30 Results. Scanning 27 tracks

Tr#  Length           CRC32      w/o NULL       ArCRC      ArCRCv2        Status (confidence)
  1  1:42.08(.107)    [662367E3] [B2E12311]     [EA5D38FD] [775088D0]     AccurateRip (7) @16, ARv2 (13) @10
  2  2:14.02(.027)    [81FA5AF0] [45EFD2C9]     [9A4B4900] [B1C9AFA2]     AccurateRip (7) @16, ARv2 (13) @10
  3  1:51.23(.307)    [044864D8] [DBE6DB39]     [69626540] [D5463F00]     AccurateRip (7) @16, ARv2 (13) @10
  4  2:44.15(.200)    [CBD2650C] [68247517]     [647D01DF] [ACABA2D4]     AccurateRip (7) @16, ARv2 (13) @10
  5  2:11.25(.333)    [CE25ACE5] [797DBCE7]     [E4FD9566] [4F6BECC0]     AccurateRip (7) @16, ARv2 (14) @9
  6  1:03.45(.600)    [E32EC012] [5CAE6149]     [0C01A61D] [991BF3E6]     AccurateRip (7) @16, ARv2 (14) @9
  7  2:13.62(.827)    [5AC41B97] [880B7BBF]     [45581A36] [44288B2D]     AccurateRip (7) @16, ARv2 (14) @9
  8  2:00.33(.440)    [C1AA399C] [0D5ED5A1]     [957F3926] [0F9426E1]     AccurateRip (7) @16, ARv2 (14) @9
  9  1:33.32(.427)    [B8980DC8] [24A9B468]     [ED348360] [F2BA3F51]     AccurateRip (7) @16, ARv2 (14) @9
 10  1:07.25(.333)    [39B136F1] [8707494B]     [7DD2986B] [FD16DA61]     AccurateRip (7) @16, ARv2 (14) @9
 11  1:54.10(.133)    [E5B96A4F] [2AAC5D3D]     [88B297A4] [47E513A5]     AccurateRip (7) @16, ARv2 (14) @9
 12  2:35.33(.440)    [9D2E7B39] [A767A7E7]     [1BC763D1] [60BC436E]     AccurateRip (7) @16, ARv2 (14) @9
 13  1:46.42(.560)    [31C6F759] [68CDB833]     [922D8653] [FE20B994]     AccurateRip (7) @16, ARv2 (14) @9
 14  1:28.10(.133)    [53D0EC67] [4FC9C0A4]     [759D777A] [DF227CA1]     AccurateRip (7) @16, ARv2 (14) @9
 15  2:20.10(.133)    [BF40562D] [A54C429A]     [A7BBA789] [86207002]     AccurateRip (7) @16, ARv2 (14) @9
 16  2:05.05(.067)    [19D8CD26] [0DE20C43]     [DACA5F2B] [48477D67]     AccurateRip (7) @16, ARv2 (14) @9
 17  1:32.20(.267)    [38107C8E] [06D5F8F1]     [B237857B] [488A3276]     AccurateRip (7) @16, ARv2 (14) @9
 18  2:16.50(.667)    [99E7066D] [42EF7E18]     [7D768F62] [53CA3ED4]     AccurateRip (7) @16, ARv2 (14) @9
 19  2:07.05(.067)    [7E4BA35E] [BC67793C]     [3CA752E6] [90FDF352]     AccurateRip (7) @16, ARv2 (14) @9
 20  2:05.43(.573)    [D423AE40] [061841B8]     [AE8037F5] [E9DA2D34]     AccurateRip (7) @16, ARv2 (14) @9
 21  2:04.67(.893)    [3FA94335] [B8D5EA2B]     [CDF39E6E] [E2F7A92E]     AccurateRip (7) @15, ARv2 (14) @9
 22  1:11.58(.773)    [3F4D8D8C] [80115643]     [8ED9CE3B] [CC30115E]     AccurateRip (7) @15, ARv2 (14) @9
 23  1:33.35(.467)    [8BF23419] [975850E0]     [29CF2F8B] [E8B0492E]     AccurateRip (7) @14, ARv2 (14) @9
 24  2:08.30(.400)    [CCE46DAC] [4E2ABB78]     [0D8EF614] [BF29E0B7]     AccurateRip (7) @15, ARv2 (13) @10
 25  1:40.62(.827)    [36BA8C3C] [07A01218]     [806DFE13] [FECB81F9]     AccurateRip (7) @16, ARv2 (13) @10
 26  1:59.25(.333)    [1D613C64] [F19E717A]     [A9FEC4DC] [50AB669E]     AccurateRip (7) @16, ARv2 (14) @9
 27  1:16.18(.240)    [3C1F34F7] [5D41C38C]     [ACDD0CBF] [2673EA15]     AccurateRip (7) @15, ARv2 (14) @8
```
> **NOTE:** Confidence column shows we have AccurateRip showing 7 others with the same checksum

### Credit

Credit must go to:
  - The authors of ARCue.pl and ARFlac.pl
  - Omy for the port to C code
