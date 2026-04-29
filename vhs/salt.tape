# ============================================================================
# salt.tape
# ============================================================================
# Prerequisites:
#   - Launch VHS from an environment where Require can already find the
#     binaries; hidden PATH exports do not affect Require preflight
# ============================================================================
# --- Binary prerequisites ---------------------------------------------------
Require salt

# --- Terminal settings -------------------------------------------------------
Set Shell bash
Set FontSize 18
# Set FontFamily "Ubuntu"
Set Width 1280
Set Height 720
Set TypingSpeed 50ms
Set Theme "Solarized Dark Higher Contrast"
Set Padding 20
Set Margin 60
Set MarginFill "#073642"
Set BorderRadius 10
Set Framerate 60
Set PlaybackSpeed 1.5

Output vhs/salt-demo.gif
Output vhs/salt-demo.mp4

# --- Hidden setup (not recorded) --------------------------------------------
Hide
Set TypingSpeed 0ms
Type "export PATH=./bin:$PATH"
Enter
Sleep 100ms
Type "export PS1='$ '"
Enter
Sleep 100ms
Type "clear"
Enter
Sleep 300ms
Set TypingSpeed 50ms
Show

# salt usage demo
Sleep 500ms
Type "./bin/salt --help"
Enter
Wait@3s /Options:|usage:/

Type "./bin/salt --version"
Enter
Wait@3s /salt [0-9]+\.[0-9]+\.[0-9]+/
