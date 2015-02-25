<?php

/**
 * @file
 * Forms to migrate books.
 */

/**
 * Form to define parameters for moving image collections.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal from state
 *
 * @return array
 *   Drupal form
 */
function limerick_migrate_image_form($form, &$form_state) {
  $form = array();
  $form['source_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Image Directory"),
    '#description' => t('Full path to postcard directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created book'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which images are being added'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Images"),
  );
  return $form;
}

/**
 * Validation handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_image_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['source_dir'])) {
    form_set_error('source_dir', t('Not a valid path'));
  }
}

/**
 * Submit handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_image_form_submit($form, &$form_state) {
  $namespace = $form_state['values']['namespace'];
  $collection_pid = $form_state['values']['collection'];
  $source_path = $form_state['values']['source_dir'];
  $dh = opendir($source_path);
  while ($filename = readdir($dh)) {
    $ext = pathinfo($filename, PATHINFO_EXTENSION);
    if (in_array($ext, array('tiff', 'tif'))) {
      $tiffs[] = "$source_path/$filename";
    }
  }
  $batch = array(
    'title' => t('Adding Images'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_image.inc',
    'progress_message' => t('@current of @total operations.'),
  );
  foreach ($tiffs as $tiff) {
    $batch['operations'][] = array('limerick_create_image', array(
        'tiff' => $tiff,
        'collection_pid' => $collection_pid,
        'namespace' => $namespace,
      ));
  }
  batch_set($batch);
  batch_process();
}

/**
 * Creates individual image object.
 *
 * @global user $user
 *
 * @param string $tiff
 *   Path to
 * @param string $collection_pid
 *   PID of collection
 * @param string $namespace
 *   Namespace for ingested object
 */
function limerick_create_image($tiff, $collection_pid, $namespace) {
  module_load_include('inc', 'limerick', 'includes/utilities');
  global $user;
  $tuque = new IslandoraTuque();
  $basename = pathinfo($tiff, PATHINFO_FILENAME);
  $dirname = pathinfo($tiff, PATHINFO_DIRNAME);
  $mods_path = "$dirname/MODS/{$basename}.xml";
  $object = $tuque->repository->constructObject($namespace);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $collection_pid);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:sp_large_image_cmodel');
  $object->label = pathinfo($tiff, PATHINFO_FILENAME);
  $object->owner = $user->name;
  // Add OBJ.
  $ds = $object->constructDatastream('OBJ', 'M');
  $object->ingestDatastream($ds);
  $ds->setContentFromFile($tiff, FALSE);
  $ds->label = 'OBJ';
  $ds->mimetype = "image/tiff";
  // Add mods.
  limerick_add_mods($object, $mods_path);
  islandora_add_object($object);
}
