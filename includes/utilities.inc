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
    $raw_mods = file_get_contents($mods_path);
    $mods_doc = new DOMDocument();
    $mods_doc->loadXML($raw_mods);
    $mods_xpath = new DOMXPath($mods_doc);
    $mods_xpath->registerNamespace('m', 'http://www.loc.gov/mods/v3');
    $object->label = $mods_xpath->evaluate('string(//m:mods/m:titleInfo/m:title/text())');
    $mods = $object->constructDatastream('MODS');
    $mods->label = 'MODS';
    $mods->mimetype = 'text/xml';
    $mods->setContentFromString($raw_mods);
    $object->ingestDatastream($mods);
  }
}

