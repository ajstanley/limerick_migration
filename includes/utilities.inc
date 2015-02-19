<?php

/*
  /**
 * @file
 * Utiltiy functions for migration.
 */

/**
 * Adds MODS datastram to object.
 *
 * @param AbstractObject $object
 *   Book Object
 * @param string $mods_path
 *   Path to MODS file
 */
function limerick_add_mods(&$object, $mods_path) {
  if (is_file($mods_path)) {
    $mods = $object->constructDatastream('MODS');
    $mods->label = 'MODS';
    $mods->mimetype = 'text/xml';
    $mods->setContentFromFile($mods_path);
    $object->ingestDatastream($mods);
  }
}