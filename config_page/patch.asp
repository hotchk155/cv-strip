<html>
<meta http-equiv='cache-control' content='no-cache'>
<meta http-equiv='expires' content='0'>
<meta http-equiv='pragma' content='no-cache'>
<head>
<style>
body {
    font-family: arial;
}

table {
    font-family: arial;
    font-size: 12px;
    margin-top: 0px;
    margin-bottom: 0px;
    margin-right: 0px;
    margin-left: 0px;
    padding: 0px;
	line-height: 1;
}
</style>
<script language="Javascript" src="cvocd.js">
</script>
</head>

<body>

<h1>CV.OCD SysEx Builder</h1>
<!-- #include file="patch.inc" -->
<hr>
<table width=800><tr><td>
<center>
<img width=600 src="img/cvocd.gif">
</center>
<hr>
<form name="sysex_file" action="patch.asp?file=true" enctype="multipart/form-data" method="post">
<table>
<tr><td>You can load an existing CV.OCD sysex into this form, or create a new one from scratch<br>
Click <a href="https://github.com/hotchk155/cvocd.a/blob/master/patches/cvocd_default.syx?raw=true">here</a> to download the default patch (as loaded on a new CV.OCD)
</td>
<td>:</td>
<td><input type="file" name="file" value="File"></td>
<td><input type="submit" value="Load SysEx Into Form"></td>
<tr>
</table>
</form>
<form id="sysex_form" action="get_sysex.asp" method="post">

<input type="hidden" name="sysex_data" id="sysex_data" value="<%=strSysex%>">

<hr>
<p><b>Globals</b> (TIP: hover mouse over field names for a description)</p>
<table id="global_settings">
<tr>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="Select the MIDI channel to use for all mapings that use MIDI channel '(default)'">Default Channel</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="Select the millsecond duration for gate outputs that are set as '(trig)'">Default Trigger</td>
</tr>
</table>
<hr>

<p><b>Note Inputs</b></p>
<table id="note_inputs">
<tr>
<td></td>
<td title="Select whether this note input is enabled and listening to MIDI.&#10;You should disable any inputs you don't need.">Enable</td>
<td title="Select the MIDI channel that this note input will listen to">Channel</td>
<td title="When multiple notes are held, this setting determines which of the notes should be be played on the outputs">Mode</td>
<td title="You can limit the range of notes that this note input will play (for example to split the keyboard between two outputs)&#10;Select min note = 0 and max note = 127 to play any note on the selected MIDI channel">Min.Note</td>
<td title="Maximum note of the range. If you select 'single' then only the single note selected in min note will play.">Max.Note</td>
<td title="You can filter notes that will play on the input based on their MIDI note velocity.&#10;Only notes that are equal or higher in velocity can play.">Min Vel.</td>
<td title="Select the pitch bend range for this note input.">Pitch Bend</td>
</tr>
</table>


<hr>
<p><b>CV outputs</b></p>
<table id="cv_outputs">
<tr>
<td></td>
<td title="Select the source of the note or voltage level that this CV output will play.">CV Source</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select a note input, this decides which note (or other parameter)&#10;from that note input will be mapped to the CV output">Note input</td>
<td title="If you select a note input, you can transpose the output note CV.&#10;This only works for notes (not velocity, bend etc)">Transpose</td>
<td title="For note pitch CV this is the voltage scaling scheme">Scheme</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select a MIDI CC as input, you specify the channel here">Chan</td>
<td title="If you select a MIDI CC as input, you specify the CC number here">CC#</td>
<td title="For non-musical note CV this is the full voltage range">Range</td>
</tr>
</table>


<hr>
<p><b>Gate Outputs</b></p>
<table id="gate_outputs">
<tr>
<td></td>
<td title="Select the source of event that will activate this gate output">Source</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select a note input, the gate can activate when&#10;a note is played from that input">Note input event</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select MIDI note or MIDI CC to activate the gate output&#10;then you can select the MIDI channel here">Chan</td>
<td title="If you select MIDI note to activate the output, select the note value here">Min.Note</td>
<td title="Select a single note, or upper limit of a range of notes to activate the output when received">Max.Note</td>
<td title="The output can activate only when notes are received that are equal or higher than this velocity">Min Vel.</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select MIDI CC to activate the output, select the CC number here">CC#</td>
<td title="If you select MIDI CC to activate the output, select the switching threshold here">Switch@</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="If you select MIDI clock to activate the output, select the clock rate (divider) here">Ck.Rate</td>
<td title="You can also offset MIDI clock outputs so they activate 'late' by a number of ticks (There are 24 ticks per beat)">Tk.Ofs</td>
<td>&nbsp;&nbsp;&nbsp;</td>
<td title="Select (trig) to pulse output with the default trigger time or&#10;select (gate) to have the output switch on until the activating condition is over (e.g. note released)&#10;You can also set a specific trigger time for this output">Trig</td>
</tr>
</table>

<p>&dagger;&nbsp;Some settings have specific firmware requirements. Make sure you have an <a href="firmwares.html">appropriate firmware version</a> to use settings marked with the &dagger; symbol</p>

<hr>
<input type="button" onclick="javascript:syx_submit();" value="I WANT YOUR SYSEX!">


</td></tr></table>

</form>

<script>
// Instantiate helper objects tree for building and
// managing the form
let patch = new Patch();

// Check for a HTML element contain the comma delimited
// sysex data with which the helper objects can be initialised
let syx_string = document.getElementById("sysex_data").value;
let syx = syx_string.split(",")
if(syx.length>0) {
	for(let o in syx) {
		syx[o] = parseInt(syx[o],10);
	}
	patch.unsyxify(syx);
}

// Populate the tables in the form
patch.render();

// This function is called when the submit button is pressed.
// It pulls data from the helper objects and stuffs it, as a
// comma-delimited string, into a HTML element for posting 
// back up to the server
function syx_submit() {
	document.getElementById("sysex_data").value = patch.syxify();
	document.getElementById("sysex_form").submit();
}
</script>
</body>
</html>