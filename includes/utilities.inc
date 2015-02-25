<?php

/**
 * @file
 * Utility migration functions.
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
    $dom = new DOMDocument();
    $dom->loadXML($raw_mods);
    $xpath = new DomXpath($dom);
    $titles = $xpath->query('//mods/titleInfo/title');
    $label = $titles->item(0)->nodeValue;
    if ($label) {
      $object->label = $label;
    }
    $mods = $object->constructDatastream('MODS');
    $mods->label = 'MODS';
    $mods->mimetype = 'text/xml';
    $mods->setContentFromString($raw_mods);
    $object->ingestDatastream($mods);
  }
}
