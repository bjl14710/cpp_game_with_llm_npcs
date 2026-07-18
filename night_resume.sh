#!/bin/bash

# CONFIGURATION: Set the hour your token pool resets (e.g., 05 for 5:00 AM)
RESET_HOUR=5
TARGET_WINDOW_MET=false

echo "🚀 Claude Code Multi-Window Sleep Watcher started."
echo "Target reset window hour configuration: $RESET_HOUR:00"

while [ "$TARGET_WINDOW_MET" = false ]; do
    CURRENT_HOUR=$(date +%H)
    
    # Check if the current time matches or has passed your known reset hour
    if [ "$CURRENT_HOUR" -ge "$RESET_HOUR" ]; then
        echo "⏰ Reset hour reached! Sending signals to all windows..."
        TARGET_WINDOW_MET=true
    else
        echo "💤 Tokens not yet reset (Current Hour: $CURRENT_HOUR). Waiting 10 minutes..."
        sleep 600 # Wait exactly 10 minutes (600 seconds)
    fi
done

# Clear safety buffer: Wait 1 extra minute to guarantee server-side resets take effect
echo "⏳ Applying safety buffer before typing..."
sleep 60

# AUTOMATION: Target every window EXCEPT the one running this script
echo "⌨️ Injecting 'please continue' into all Claude Code windows..."
osascript -e '
tell application "Terminal"
    activate
    set allWindows to windows
    -- Get the ID of the window running this script to skip it
    set scriptWindowID to id of front window 
    
    repeat with win in allWindows
        if id of win is not scriptWindowID then
            -- Bring this specific Claude window to the front
            set frontmost of win to true
            delay 0.2
            
            tell application "System Events"
                keystroke "please continue"
                key code 36 -- Enter key
            end tell
            delay 0.5
        end if
    end repeat
end tell'

echo "✅ 'please continue' signal successfully sent to all front tabs."

