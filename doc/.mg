# Micro Emacs default configuration file
auto-indent-mode
make-backup-files
auto-fill-mode
set-fill-column 72
#set-default-mode c
#column-number-mode
auto-execute *.c c-mode
# Follow log files like tail -f, a * also matches subdirectories
auto-execute *.log auto-revert-tail-mode
auto-execute /var/log/* auto-revert-tail-mode
