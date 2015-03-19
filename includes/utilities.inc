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
    $raw_mods = trim(file_get_contents($mods_path));
    $dom = new DOMDocument();
    $dom->loadXML($raw_mods);
    $xpath = new DomXpath($dom);
    $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
    $titles = $xpath->query('//mods:mods/mods:titleInfo/mods:title');
    if ($titles->length > 0) {
      $label = $titles->item(0)->nodeValue;
    }
    else {
      $titles = $xpath->query('//mods/titleInfo/title');
      $label = $titles->item(0)->nodeValue;
      $label = html_entity_decode($label);
    }
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

/**
 * Gathers DC information from handles
 *
 * @param url $handle
 *
 * DSpace handle
 * @param AbstractObject $object
 *   Object representing digital asset
 */
function limerick_harvest_dc($handle, $object) {
  $sample_handle = "http://ulir.ul.ie/handle/10344/1007";
  module_load_include('inc', 'islandora', 'includes/dublin_core');
  $current = NULL;
  if ($object['DC']) {
    $current = $object['DC']->content;
  }
  $dc = new DublinCore();
  $dom = new DOMDocument();
  @$dom->loadHTMLFile("$handle?show=full");
  $xpath = new DomXpath($dom);
  $metas = $xpath->query('//meta');
  foreach ($metas as $meta) {
    $name = $meta->getAttribute('name');
    if (substr($name, 0, 3) == 'DC.') {
      $name = str_replace('DC.', 'dc:', $name);
      $value = $meta->getAttribute('content');
      $dc->addElement($name, $value);
    }
  }
  $dc_xml = $dc->asXML();
  if ($object['DC']) {
    $object['DC']->content = $dc_xml;
  }
  else{
    $dc = $object->constructDatastream('DC');
    $dc->label = 'DC';
    $dc->mimetype = 'text/xml';
    $dc->setContentFromString($dc_xml);
    $object->ingestDatastream($dc);
  }
}