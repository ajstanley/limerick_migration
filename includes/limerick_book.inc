<?php

/**
 * @file
 * Forms to migrate books.
 */

/**
 * Form to define parameters for moving Books.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal from state
 *
 * @return array
 *   Drupal form
 */
function limerick_migrate_book_form($form, &$form_state) {
  $form = array();
  $form['book_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Book directory"),
    '#description' => t('Full path to book directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created book'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which book is being added'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Book"),
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
function limerick_migrate_book_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['book_dir'])) {
    form_set_error('book_dir', t('Not a valid path'));
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
function limerick_migrate_book_form_submit(&$form, &$form_state) {
  module_load_include('inc', 'limerick', 'includes/utilities');
  $namespace = $form_state['values']['namespace'];
  $collection_pid = $form_state['values']['collection'];
  $book_path = $form_state['values']['book_dir'];
  $path_parts = explode('/', $book_path);
  $title = array_pop($path_parts);
  $book_dir_path = implode('/', $path_parts);
  $book_object = limerick_contruct_book($namespace, $title, $collection_pid);
  $mods_path = "$book_dir_path/MODS/$title.xml";
  limerick_add_mods($book_object, $mods_path);
  $new_book = islandora_add_object($book_object);
  limerick_add_pages($new_book->id, $book_path);
}

/**
 * Creates book object.
 *
 * @global type $user
 *
 * @param string $namespace
 *   Namepspoace for created object
 * @param string $title
 *   Book Title
 * @param string $parent_collection
 *   Collection pid for created book
 *
 * @return AbstractObject
 *   Book Object
 */
function limerick_contruct_book($namespace, $title, $parent_collection) {
  global $user;
  $fixed_title = str_replace('_', ' ', $title);
  $tuque = new IslandoraTuque();
  $book = $tuque->repository->constructObject($namespace);
  $book->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $parent_collection);
  $book->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:bookCModel');
  $book->label = $fixed_title;
  $book->owner = $user->name;
  return $book;
}

/**
 * Creates book pages and associates them with book object.
 *
 * @param string $book_pid
 *   PID of book
 * @param string $book_path
 *   Fole path to book directory
 */
function limerick_add_pages($book_pid, $book_path) {
  $dh = opendir($book_path);
  while ($filename = readdir($dh)) {
    $ext = pathinfo($filename, PATHINFO_EXTENSION);
    if (in_array($ext, array('tiff', 'tif'))) {
      $tiffs[] = "$book_path/$filename";
    }
  }

  $batch = array(
    'title' => t('Adding book pages'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_book.inc',
    'progress_message' => t('@current of @total pages created.'),
  );
  $page_number = 1;
  foreach ($tiffs as $tiff) {
    $batch['operations'][] = array('limerick_create_page', array(
        'tiff' => $tiff,
        'page_number' => $page_number++,
        'book_pid' => $book_pid));
  }
  $batch['operations'][] = array('limerick_update_book_thumbnail', array('book_pid' => $book_pid));
  batch_set($batch);
  batch_process();
}

/**
 * Creates individual page objects.
 *
 * @global type $user
 *
 * @param string $tiff
 *   File path to page image
 * @param string $page_number
 *   Page number to be assigned to page object
 * @param string $book_pid
 *   PID of book to which page is associated
 */
function limerick_create_page($tiff, $page_number, $book_pid) {
  module_load_include('inc', 'islandora_ocr', 'includes/derivatives');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $namespace = substr($book_pid, 0, strpos($book_pid, ":"));
  global $user;
  $tuque = new IslandoraTuque();
  $repository = $tuque->repository;
  // Create object.
  $object = $repository->constructObject($namespace);
  $object->owner = $user->name;
  $object->label = pathinfo($tiff, PATHINFO_FILENAME);
  $ds_label = pathinfo($tiff, PATHINFO_BASENAME);
  islandora_paged_content_update_datastream($object, $tiff, 'OBJ', $ds_label, NULL, 'M', FALSE);
  $rels_ext = $object->relationships;
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:pageCModel');
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageOf', $book_pid);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', $page_number, TRUE);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageNumber', $page_number, TRUE);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSection', '1', TRUE);
  islandora_paged_content_set_relationship($rels_ext, FEDORA_RELS_EXT_URI, 'isMemberOf', $book_pid);
  islandora_paged_content_page_derive_image_datastreams($object);
  $ingested_object = islandora_add_object($object);
  $options = array(
    'preprocess' => FALSE,
  );
  islandora_ocr_derive_datastreams($ingested_object['OBJ'], $options);
}

/**
 * Updates thumbnail image for book object.
 *
 * @param string $book_pid
 *   Identifier for book object
 */
function limerick_update_book_thumbnail($book_pid) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $book_object = islandora_object_load($book_pid);
  islandora_paged_content_update_paged_content_thumbnail($book_object);
}
