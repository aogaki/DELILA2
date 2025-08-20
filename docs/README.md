# DELILA2 Documentation

This directory contains comprehensive API documentation and architecture diagrams for the DELILA2 data acquisition system.

## Contents

### API Documentation
- **Doxyfile**: Doxygen configuration for API reference generation
- **api/**: Generated API documentation (created by `make docs`)

### Architecture Diagrams
- **diagrams/**: PlantUML source files and generated diagrams
  - `system_overview.puml`: High-level system architecture
  - `digitizer_class_hierarchy.puml`: Digitizer library class structure
  - `network_architecture.puml`: Network library design
  - `data_flow.puml`: Data flow through the system

### Generated Content
- **images/**: Converted PlantUML diagrams (PNG format)

## Building Documentation

### Prerequisites

#### Required
- **Doxygen**: For API documentation generation
  ```bash
  # Ubuntu/Debian
  sudo apt install doxygen
  
  # macOS
  brew install doxygen
  
  # CentOS/RHEL
  sudo dnf install doxygen
  ```

#### Optional (for diagrams)
- **Java**: Required for PlantUML
- **PlantUML**: For UML diagram generation
  ```bash
  # Ubuntu/Debian
  sudo apt install plantuml
  
  # macOS
  brew install plantuml
  
  # Manual installation
  wget https://sourceforge.net/projects/plantuml/files/plantuml.jar/download -O plantuml.jar
  export PLANTUML_JAR_PATH=/path/to/plantuml.jar
  ```

### Build Commands

```bash
# Configure with documentation enabled
mkdir build && cd build
cmake -DBUILD_DOCUMENTATION=ON ..

# Generate all documentation
make docs

# Generate diagrams only (if PlantUML available)
make diagrams

# Clean documentation
make docs-clean

# Open documentation in browser (Linux/macOS)
make docs-open
```

### Quick Start (No CMake)

```bash
# Generate documentation directly with Doxygen
cd docs
doxygen Doxyfile

# Generate diagrams manually (if PlantUML available)
java -jar /path/to/plantuml.jar -tpng diagrams/*.puml
```

## Documentation Structure

### Generated API Documentation

The generated documentation includes:

- **Main Page**: Project overview from README.md
- **Modules**: Organized by library (Digitizer, Network, Monitor)
- **Classes**: Detailed class documentation with inheritance diagrams
- **Files**: Source code browser with syntax highlighting
- **Examples**: Code examples from the examples/ directory

### Key Documentation Features

#### Comprehensive Interface Documentation
- **IDigitizer**: Abstract digitizer interface with detailed method documentation
- **IDecoder**: Data processing interface with performance notes
- **ZMQTransport**: Network transport with usage examples
- **DataProcessor**: Serialization layer with format specifications

#### Architecture Diagrams
- **System Overview**: Shows relationships between major components
- **Class Hierarchies**: Detailed class relationships and inheritance
- **Data Flow**: Complete data path from hardware to network
- **Network Architecture**: ZeroMQ integration and message patterns

#### Code Examples
- Embedded code examples in class documentation
- Complete usage examples from examples/ directory
- Performance notes and optimization guidelines
- Thread safety and concurrency information

## Documentation Standards

### Doxygen Comments

We use Javadoc-style comments with the following conventions:

```cpp
/**
 * @brief Brief description (one line)
 * 
 * Detailed description with multiple paragraphs.
 * Include usage information and important notes.
 * 
 * @param param_name Parameter description
 * @return Return value description
 * 
 * @par Usage Example:
 * @code{.cpp}
 * // Code example here
 * @endcode
 * 
 * @par Thread Safety:
 * Thread safety information
 * 
 * @warning Important warnings
 * @note Additional notes
 * 
 * @see RelatedClass
 * @see RelatedMethod()
 */
```

### PlantUML Diagrams

- Use `!theme plain` for consistent styling
- Include clear titles and legends
- Group related components in packages
- Use meaningful colors and layout
- Add notes for important clarifications

### File Organization

```
docs/
├── README.md                    # This file
├── Doxyfile                     # Doxygen configuration
├── CMakeLists.txt               # Build configuration
├── diagrams/                    # PlantUML sources
│   ├── system_overview.puml
│   ├── digitizer_class_hierarchy.puml
│   ├── network_architecture.puml
│   └── data_flow.puml
├── images/                      # Generated diagram images
└── api/                         # Generated API docs (gitignored)
    ├── html/                    # HTML documentation
    └── latex/                   # LaTeX output (if enabled)
```

## Integration with Build System

The documentation system is fully integrated with CMake:

- **Automatic Configuration**: Doxyfile uses CMake variables for paths
- **Dependency Management**: PlantUML diagrams generated before Doxygen
- **Installation Support**: Documentation can be packaged and installed
- **CI/CD Ready**: Can be built in automated environments

## Viewing Documentation

### Local Development
1. Build documentation: `make docs`
2. Open: `build/docs/api/html/index.html`
3. Or use: `make docs-open` (Linux/macOS)

### Online Hosting
The generated HTML can be hosted on:
- GitHub Pages
- GitLab Pages  
- Internal documentation servers
- Any static web server

## Maintenance

### Adding New Classes
1. Add comprehensive Doxygen comments to header files
2. Include usage examples and thread safety notes
3. Update relevant PlantUML diagrams
4. Regenerate documentation

### Updating Diagrams
1. Modify `.puml` files in `diagrams/`
2. Run `make diagrams` to regenerate images
3. Rebuild documentation with `make docs`

### Configuration Changes
- Modify `Doxyfile` for Doxygen settings
- Update `CMakeLists.txt` for build integration
- Adjust PlantUML settings in diagram files

## Troubleshooting

### Common Issues

**Doxygen not found**
```bash
# Install Doxygen first, then reconfigure
cmake -DBUILD_DOCUMENTATION=ON ..
```

**PlantUML diagrams not generated**
```bash
# Check Java installation
java -version

# Check PlantUML path
find /usr -name "plantuml.jar" 2>/dev/null

# Set environment variable if needed
export PLANTUML_JAR_PATH=/path/to/plantuml.jar
```

**Missing dependencies in generated docs**
- Ensure all header files have proper include paths
- Check INCLUDE_PATH in Doxyfile
- Verify all referenced files exist

### Build Issues

**CMake configuration fails**
```bash
# Clear cache and reconfigure
rm -rf build/
mkdir build && cd build
cmake -DBUILD_DOCUMENTATION=ON ..
```

**Incomplete documentation**
- Check Doxygen warnings in build output
- Verify all source files are included
- Review EXCLUDE patterns in Doxyfile

## Contributing

When contributing to DELILA2:

1. **Document all public interfaces** with comprehensive Doxygen comments
2. **Include usage examples** in class documentation
3. **Update architecture diagrams** if adding new components
4. **Test documentation generation** before submitting PRs
5. **Follow established documentation patterns** and style

The documentation is as important as the code - well-documented APIs make DELILA2 easier to use, maintain, and extend.