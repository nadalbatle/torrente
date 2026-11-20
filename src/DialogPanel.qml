// From Omacut (MIT, David Heinemeier Hansson).
import QtQuick

// Every dialog's panel: dark, a quiet border, and the corners Hyprland gives
// windows (sharp on Omarchy's default).
Rectangle {
    color: theme.background
    // The accent frame the bar's panels wear.
    border.width: 2
    border.color: theme.accent
    radius: theme.radius
}
