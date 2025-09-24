-- vim.lua
--
-- This script configures mg to use vim-like keybindings.

-- Define a new keymap for normal mode
mg.new_keymap("vim-normal")

-- Define functions for normal mode commands
function vim_move_left() mg.backchar() end
function vim_move_down() mg.forwline() end
function vim_move_up() mg.backline() end
function vim_move_right() mg.forwchar() end

function vim_insert_mode()
    mg.set_mode("fundamental")
end

-- Register the functions as mg commands
mg.defun("vim-move-left", vim_move_left)
mg.defun("vim-move-down", vim_move_down)
mg.defun("vim-move-up", vim_move_up)
mg.defun("vim-move-right", vim_move_right)
mg.defun("vim-insert-mode", vim_insert_mode)

-- Bind keys in the normal mode map
mg.bind_key("vim-normal", "h", "vim-move-left")
mg.bind_key("vim-normal", "j", "vim-move-down")
mg.bind_key("vim-normal", "k", "vim-move-up")
mg.bind_key("vim-normal", "l", "vim-move-right")
mg.bind_key("vim-normal", "i", "vim-insert-mode")

-- Define a function to enter normal mode
function vim_normal_mode()
    mg.set_mode("vim-normal")
end

-- Register the normal mode command
mg.defun("vim-normal-mode", vim_normal_mode)

-- Bind ESC in the fundamental map to enter normal mode
-- The escape key is represented as '^[', which is CCHR('[')
mg.bind_key("fundamental", "^[", "vim-normal-mode")

-- Enter normal mode by default
vim_normal_mode()
