<?php

/**
 * @file
 * Forms to migrate books.
 */

/**
 * Form to define parameters for moving entire collections.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal from state
 *
 * @return array
 *   Drupal form
 */
function limerick_migrate_collection_form($form, &$form_state) {
  $form = array();
  $form['source_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection Directory"),
    '#description' => t('Full path to collection directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created collection'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which nested collection is being added'),
  );
  $form['mods_folder'] = array(
    '#type' => 'textfield',
    '#title' => t("MODS folder"),
    '#description' => t('Path to folder holding name-linked MODS records'),
    '#required' => TRUE,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Collection"),
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
function limerick_migrate_collection_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['source_dir'])) {
    form_set_error('source_dir', t('Not a valid file path.'));
  }
  if (!islandora_object_load($form_state['values']['collection'])) {
    form_set_error('collection', t('Collection does not exist.'));
  }
  if (!is_dir($form_state['values']['mods_folder'])) {
    form_set_error('mods_folder', t('Not a valid file path.'));
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
function limerick_migrate_collection_form_submit($form, &$form_state) {
  $namespace = $form_state['values']['namespace'];
  $collection_pid = $form_state['values']['collection'];
  $source_path = $form_state['values']['source_dir'];
  $mods_path = $form_state['values']['mods_folder'];
  $dirs = array();
  $files = array();
  foreach ($iterator = new RecursiveIteratorIterator(
  new RecursiveDirectoryIterator($source_path, RecursiveDirectoryIterator::SKIP_DOTS), RecursiveIteratorIterator::SELF_FIRST) as $item) {
    $subpath = $iterator->getSubPathName();
    if ($item->isDir()) {

      if (!substr_count($subpath, 'tmpFilename') && !substr_count($subpath, '@eaDir') && !substr_count($subpath, 'thumb') && !substr_count($subpath, 'undo') && $subpath != 'MODS') {
        $dirs[] = $subpath;
      }
    }
    else {
      $ext = pathinfo($subpath, PATHINFO_EXTENSION);
      if (in_array($ext, array('tif', 'tiff'))) {
        $files[] = $subpath;
      }
    }
  }
  sort($files);
  $batch = array(
    'title' => t('Adding Collection'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_collection.inc',
    'progress_message' => t('@current of @total operations completed.'),
  );

  foreach ($dirs as $dir) {
    $batch['operations'][] = array('limerick_create_dir', array(
        'dir' => $dir,
        'namespace' => $namespace,
        'collection_pid' => $collection_pid,
        'source_path' => $source_path,
        'mods_path' => $mods_path,
      ),
    );
  }
  foreach ($files as $file) {
    $batch['operations'][] = array('limerick_create_collection_file', array(
        'file' => $file,
        'namespace' => $namespace,
        'collection_pid' => $collection_pid,
        'source_path' => $source_path,
        'mods_path' => $mods_path,
      ),
    );
  }
  batch_set($batch);
  batch_process();
}

/**
 * Creates directory object.
 *
 * @global user $user
 *
 * @param string $dir
 *   Dir name
 * @param string $namespace
 *   Namespace for created object
 * @param string $collection_pid
 *   PID of parent collection
 * @parm $source_path
 *   File path
 */
function limerick_create_dir($dir, $namespace, $collection_pid, $source_path, $mods_path) {
  module_load_include('inc', 'limerick', 'includes/utilities');
  global $user;
  // renames bad filenames that make illegal pids..
  $bad_chars = array('(', ')');
  $fixed_dir = str_replace($bad_chars, '', $dir);
  if ($fixed_dir != $dir) {
    rename($dir, $fixed_dir);
  }
  $dir = $fixed_dir;
  $dir_parts = explode('/', $dir);
  $qualifier = implode('-', $dir_parts);
  $dir_pid = "$namespace:$qualifier";
  if (islandora_object_load($dir_pid)) {
    return;
  }
  $label = array_pop($dir_parts);
  if (!$dir_parts) {
    $member_of = $collection_pid;
  }
  else {
    $member_of = $namespace . ':' . implode('-', $dir_parts);
  }
  $tuque = new IslandoraTuque();
  $object = $tuque->repository->constructObject($dir_pid);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $member_of);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:collectionCModel');
  $object->label = $label;
  $object->owner = $user->name;
  $temp_path = implode('/', $dir_parts);
  $mods_path = "$mods_path/{$label}.xml";
  limerick_add_mods($object, $mods_path);
  islandora_add_object($object);
}

/**
 * Creates object from file.
 *
 * @global user $user
 *
 * @param string $file
 *   File name
 * @param string $namespace
 *   Namespace of ingested object
 * @param syring $collection_pid
 *   PID of collection
 * @param string $source_path
 *   File Path
 */
function limerick_create_collection_file($file, $namespace, $collection_pid, $source_path, $mods_path) {
  module_load_include('inc', 'limerick', 'includes/utilities');

  global $user;
  $file_parts = explode('/', $file);
  $tuque = new IslandoraTuque();
  $label = pathinfo($file, PATHINFO_FILENAME);
  array_pop($file_parts);
  if (!$file_parts) {
    $member_of = $collection_pid;
  }
  else {
    $member_of = $namespace . ':' . implode('-', $file_parts);
  }
  $file_path = "$source_path/$file";
  $file_path_parts = explode('/', $file_path);
  array_pop($file_path_parts);
  $mods_path = "$mods_path/{$label}.xml";
  $object = $tuque->repository->constructObject($namespace);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $member_of);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:sp_large_image_cmodel');
  $object->label = $label;
  $object->owner = $user->name;
  $ds = $object->constructDatastream('OBJ', 'M');
  $ds->setContentFromFile($file_path, FALSE);
  $ds->label = 'OBJ';
  $ds->mimetype = "image/tiff";
  $object->ingestDatastream($ds);
  limerick_add_mods($object, $mods_path);
  islandora_add_object($object);
}