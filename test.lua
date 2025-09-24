-- test.lua
--
-- A simple script to test the mg.defun function.

function my_test_command()
    mg.forwline()
    mg.forwline()
    mg.forwchar()
end

mg.defun("my-test-command", my_test_command)
