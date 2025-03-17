/*
 * Copyright (c) 2021 NetFoundry. All rights reserved.
 */

package org.openziti.mobile.debug

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import org.openziti.mobile.databinding.FragmentDebugInfoBinding

/**
 * A placeholder fragment containing a simple view.
 */
class DebugInfoFragment : Fragment() {

    private lateinit var pageViewModel: PageViewModel
    private var _binding: FragmentDebugInfoBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        pageViewModel = ViewModelProvider(this)[PageViewModel::class.java]
        val dia = activity as DebugInfoActivity
        arguments?.getString(SECTION_ARG)?.let { name ->
            val info = dia.getSectionProvider(name)
            pageViewModel.setText(info?.dump(name)?.toString() ?: "nothing to see")
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {

        _binding = FragmentDebugInfoBinding.inflate(inflater, container, false)
        val root = binding.root

        val textView: TextView = binding.sectionLabel
        textView.setHorizontallyScrolling(true)
        pageViewModel.text.observe(viewLifecycleOwner, { textView.text = it })
        return root
    }

    companion object {
        const val SECTION_ARG = "section"

        @JvmStatic
        fun newInstance(section: String): DebugInfoFragment {
            val args = Bundle().apply {
                putString(SECTION_ARG, section)
            }

            return DebugInfoFragment().apply { arguments = args }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}