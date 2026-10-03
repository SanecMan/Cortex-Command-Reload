# Discord Rich Presence

Discord Rich Presence is enabled by default on Windows and can be switched off in **Settings → Misc → Discord Rich Presence**. The setting is saved with the other game settings. No account token or network service is required: the game connects to the local Discord IPC endpoint and sends the registered application ID from the client.

The status shows the main menu or the current Activity name. If Discord is closed or unavailable, the game retries in the background and continues normally. The integration publishes text presence only; it does not implement invites or join requests.
