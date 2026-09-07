using System;
using System.Runtime.InteropServices;
using System.Threading;

// Dedicated thread: SetThreadExecutionState must be released on its owning thread.
public sealed class GrayBenchmarkSession : IDisposable {
    [DllImport("kernel32.dll")] static extern uint SetThreadExecutionState(uint flags);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint from, uint to, bool attach);
    [DllImport("user32.dll")] static extern bool ShowWindowAsync(IntPtr window, int command);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr SetFocus(IntPtr window);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr window, IntPtr order, int x, int y, int w, int h, uint flags);
    readonly ManualResetEvent ready = new ManualResetEvent(false), stop = new ManualResetEvent(false);
    readonly Thread thread;
    public uint ExecutionState { get; private set; }
    public uint RestoreState { get; private set; }
    public GrayBenchmarkSession() {
        thread = new Thread(() => {
            ExecutionState = SetThreadExecutionState(0x80000003u); // continuous + system + display
            ready.Set();
            try { stop.WaitOne(); }
            finally { RestoreState = SetThreadExecutionState(0x80000000u); }
        });
        thread.IsBackground = true;
        thread.Start(); ready.WaitOne();
    }
    public void Dispose() { stop.Set(); thread.Join(); ready.Dispose(); stop.Dispose(); }
    public static bool Activate(IntPtr window, uint expectedPid) {
        uint owner, ignored;
        uint target = GetWindowThreadProcessId(window, out owner);
        if (!IsWindow(window) || owner != expectedPid || target == 0) return false;
        uint current = GetCurrentThreadId();
        uint foreground = GetWindowThreadProcessId(GetForegroundWindow(), out ignored);
        bool attachTarget = false, attachForeground = false;
        try {
            ShowWindowAsync(window, 9); // restore only this run's window
            attachTarget = target != current && AttachThreadInput(current, target, true);
            attachForeground = foreground != 0 && foreground != target && foreground != current
                && AttachThreadInput(current, foreground, true);
            SetWindowPos(window, new IntPtr(-1), 0, 0, 0, 0, 0x13); // temporary topmost, no activation
            BringWindowToTop(window);
            bool result = SetForegroundWindow(window);
            SetFocus(window);
            return result; // diagnostic only; engine telemetry is authoritative
        } finally {
            SetWindowPos(window, new IntPtr(-2), 0, 0, 0, 0, 0x13); // ordinary window again
            if (attachForeground) AttachThreadInput(current, foreground, false);
            if (attachTarget) AttachThreadInput(current, target, false);
        }
    }
}
