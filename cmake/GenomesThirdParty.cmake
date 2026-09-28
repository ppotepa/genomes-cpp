function(genomes_mark_third_party target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()

    # Keep vendor warning policy separate from Genomes-owned targets.  We do
    # not add /WX or -Werror here: upstream targets own their flags.
    set_property(TARGET "${target_name}" PROPERTY GENOMES_THIRD_PARTY TRUE)
endfunction()
